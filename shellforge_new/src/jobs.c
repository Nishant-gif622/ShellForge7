/*
 * jobs.c
 *
 * Implementation of job control: a small global table that
 * remembers every background or stopped pipeline launched from
 * the shell so the user can refer to them later with jobs,
 * fg and bg.
 *
 * The table is filled in by the executor at fork time and
 * updated by signal handlers when children stop / continue /
 * terminate.  No allocation - everything is stored in static
 * arrays so the data is safe to touch from a signal handler.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "jobs.h"
#include "executor.h"

/* =========================================================
   STATIC JOB TABLE
   ========================================================= */

static job_t jobs_table[MAX_JOBS];
static int   job_count     = 0;
static int   next_job_id   = 1;
static pid_t shell_pgid    = 0;

/* =========================================================
   FOREGROUND PROCESS GROUP
   ---------------------------------------------------------
   fg() temporarily moves a job into the shell's controlling
   terminal so ^C / ^Z reach it.  The shell itself stays in
   its own group.
   ========================================================= */

void job_set_shell_pgid(pid_t p) { shell_pgid = p; }
pid_t job_get_shell_pgid(void)   { return shell_pgid; }

/* =========================================================
   INIT
   ========================================================= */

void jobs_init(void)
{
    memset(jobs_table, 0, sizeof(jobs_table));
    job_count   = 0;
    next_job_id = 1;
}

/* =========================================================
   ADD A JOB TO THE TABLE
   ========================================================= */

int job_add(pid_t pgid, const char *command, job_state_t state)
{
    if (job_count >= MAX_JOBS)
    {
        fprintf(stderr, "jobs: job table full\n");
        return -1;
    }

    job_t *job = &jobs_table[job_count];

    job->job_id = next_job_id++;
    job->pgid   = pgid;
    job->state  = state;

    if (command != NULL)
    {
        strncpy(job->command, command, MAX_JOB_COMMAND - 1);
        job->command[MAX_JOB_COMMAND - 1] = '\0';
    }
    else
    {
        job->command[0] = '\0';
    }

    job_count++;

    return job->job_id;
}

/* =========================================================
   LOOKUP
   ========================================================= */

job_t *job_find(int job_id)
{
    for (int i = 0; i < job_count; i++)
    {
        if (jobs_table[i].job_id == job_id)
            return &jobs_table[i];
    }
    return NULL;
}

job_t *job_find_by_pgid(pid_t pgid)
{
    for (int i = 0; i < job_count; i++)
    {
        if (jobs_table[i].pgid == pgid)
            return &jobs_table[i];
    }
    return NULL;
}

/* =========================================================
   REMOVE A JOB
   ========================================================= */

void job_remove(int job_id)
{
    int i, j;

    for (i = 0; i < job_count; i++)
    {
        if (jobs_table[i].job_id == job_id)
        {
            /* shift everything down to keep the table dense */
            for (j = i; j < job_count - 1; j++)
                jobs_table[j] = jobs_table[j + 1];

            job_count--;
            memset(&jobs_table[job_count], 0, sizeof(job_t));
            return;
        }
    }
}

/* =========================================================
   STATE TRANSITIONS
   ========================================================= */

void job_stop(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);
    if (job != NULL)
        job->state = JOB_STOPPED;
}

void job_continue(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);
    if (job != NULL)
        job->state = JOB_RUNNING;
}

void job_done(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);
    if (job != NULL)
        job->state = JOB_DONE;
}

/* =========================================================
   REAP CHILDREN
   ---------------------------------------------------------
   Watches for stopped / exited children and updates the
   table accordingly.  Done jobs stay in the table until
   they are printed by jobs_print() and then removed.
   ========================================================= */

void reap_jobs(void)
{
    int     status;
    pid_t   pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFSTOPPED(status))
        {
            /* Child (or its whole group) was stopped, e.g. ^Z */
            job_stop(pid);
        }
        else if (WIFEXITED(status) ||
                 WIFSIGNALED(status))
        {
            job_done(pid);

            /* WIFSIGNALED with SIGPIPE / SIGTERM should not
               leave a "Done" entry forever - reap once. */
            job_t *job = job_find_by_pgid(pid);
            if (job != NULL)
                job_remove(job->job_id);
        }
    }
}

/* =========================================================
   jobs - LIST BACKGROUND JOBS
   ========================================================= */

int builtin_jobs(command_t *cmd)
{
    (void)cmd;

    reap_jobs();

    printf("\n");
    printf("%-4s %-10s %-20s %s\n",
           "[ID]", "State", "PGID", "Command");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < job_count; i++)
    {
        const char *state = "Unknown";
        switch (jobs_table[i].state)
        {
            case JOB_RUNNING: state = "Running"; break;
            case JOB_STOPPED: state = "Stopped"; break;
            case JOB_DONE:    state = "Done";    break;
        }

        printf("[%-2d] %-10s %-20d %s\n",
               jobs_table[i].job_id,
               state,
               jobs_table[i].pgid,
               jobs_table[i].command);
    }

    printf("\n");
    return 0;
}

/* =========================================================
   fg - BRING A JOB TO THE FOREGROUND
   ========================================================= */

int builtin_fg(command_t *cmd)
{
    int job_id;

    if (cmd->argc < 2)
    {
        fprintf(stderr, "fg: usage: fg <job_id>\n");
        return -1;
    }

    job_id = atoi(cmd->argv[1]);

    job_t *job = job_find(job_id);
    if (job == NULL)
    {
        fprintf(stderr, "fg: no such job [%d]\n", job_id);
        return -1;
    }

    /* If the job is stopped, resume it first */
    if (job->state == JOB_STOPPED)
    {
        kill(-job->pgid, SIGCONT);
        job_continue(job->pgid);
    }

    /* Hand the controlling terminal to the job's group
       so ^C / ^Z reach it, not the shell. */
    if (shell_pgid != 0)
        tcsetpgrp(STDIN_FILENO, job->pgid);

    printf("%s\n", job->command);

    /* Wait for any process in that group to finish or stop */
    int status;
    pid_t pid;
    while ((pid = waitpid(-job->pgid, &status, WUNTRACED)) > 0)
    {
        if (WIFSTOPPED(status))
        {
            job_stop(pid);
            /* shell regains the terminal */
            if (shell_pgid != 0)
                tcsetpgrp(STDIN_FILENO, shell_pgid);
            break;
        }
        else
        {
            job_done(pid);
            job_t *j = job_find_by_pgid(pid);
            if (j != NULL)
            {
                if (j->pgid == job->pgid)
                    job_remove(j->job_id);
            }
        }
    }

    /* Shell regains the terminal */
    if (shell_pgid != 0)
        tcsetpgrp(STDIN_FILENO, shell_pgid);

    return 0;
}

/* =========================================================
   bg - RESUME A STOPPED JOB IN THE BACKGROUND
   ========================================================= */

int builtin_bg(command_t *cmd)
{
    int job_id;

    if (cmd->argc < 2)
    {
        fprintf(stderr, "bg: usage: bg <job_id>\n");
        return -1;
    }

    job_id = atoi(cmd->argv[1]);

    job_t *job = job_find(job_id);
    if (job == NULL)
    {
        fprintf(stderr, "bg: no such job [%d]\n", job_id);
        return -1;
    }

    if (kill(-job->pgid, SIGCONT) < 0)
    {
        perror("bg: kill");
        return -1;
    }

    job_continue(job->pgid);

    printf("[%d] %s &\n", job->job_id, job->command);

    return 0;
}
