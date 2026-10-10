#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include "parser.h"

/* =========================================================
   JOB CONTROL
   ---------------------------------------------------------
   A job is one or more processes that together form a
   unit the user can refer to with the builtins
       jobs    list background / stopped jobs
       fg <id> bring a job to the foreground
       bg <id> resume a stopped job in the background
   ========================================================= */

#define MAX_JOBS           64
#define MAX_JOB_COMMAND    256

typedef enum
{
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct
{
    int          job_id;          /* 1, 2, 3 ...                         */
    pid_t        pgid;            /* process group ID                    */
    job_state_t  state;           /* running / stopped / done            */
    char         command[MAX_JOB_COMMAND];
} job_t;

/* Table management */
void   jobs_init(void);
int    job_add(pid_t pgid, const char *command, job_state_t state);
job_t *job_find(int job_id);
job_t *job_find_by_pgid(pid_t pgid);
void   job_remove(int job_id);
void   jobs_print(void);

/* State transitions (called from signal handlers / builtins) */
void   job_stop(pid_t pgid);
void   job_continue(pid_t pgid);
void   job_done(pid_t pgid);

/* Reap any children that have exited. Called before each prompt. */
void   reap_jobs(void);

/* Builtin handlers */
int    builtin_jobs(command_t *cmd);
int    builtin_fg(command_t *cmd);
int    builtin_bg(command_t *cmd);

#endif
