#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>

#include "builtin.h"
#include "jobs.h"   /* for builtin_jobs / builtin_fg / builtin_bg */


/* =========================================================
   BUILTIN: cd
   ========================================================= */

static int builtin_cd(command_t *cmd)
{
    const char *directory;

    if (cmd->argc == 1)
    {
        directory = getenv("HOME");
        if (directory == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return -1;
        }
    }
    else if (cmd->argc == 2)
    {
        directory = cmd->argv[1];
    }
    else
    {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (chdir(directory) != 0)
    {
        perror("cd");
        return -1;
    }

    return 0;
}


/* =========================================================
   BUILTIN: pwd
   ========================================================= */

static int builtin_pwd(command_t *cmd)
{
    char current_directory[4096];

    if (cmd->argc > 1)
    {
        fprintf(stderr, "pwd: too many arguments\n");
        return -1;
    }

    if (getcwd(current_directory, sizeof(current_directory)) == NULL)
    {
        perror("pwd");
        return -1;
    }

    printf("%s\n", current_directory);
    return 0;
}


/* =========================================================
   BUILTIN: echo
   ========================================================= */

static int builtin_echo(command_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++)
    {
        printf("%s", cmd->argv[i]);
        if (i < cmd->argc - 1)
            printf(" ");
    }
    printf("\n");
    return 0;
}


/* =========================================================
   BUILTIN: exit
   ========================================================= */

static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }
    return 1;
}


/* =========================================================
   BUILTIN: clear           (NEW)
   ---------------------------------------------------------
   Clears the terminal screen using ANSI escape codes so the
   shell works without depending on the external `clear`
   utility or `ncurses`.
   ========================================================= */

static int builtin_clear(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "clear: too many arguments\n");
        return -1;
    }

    /* "\033[2J"  - erase entire screen
       "\033[H"   - move cursor to home                 */
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}


/* =========================================================
   BUILTIN: whoami          (NEW)
   ---------------------------------------------------------
   Shows the effective username of the shell process by
   looking up the current uid in the system password
   database (falling back to the uid number if no entry
   exists, e.g. inside containers).
   ========================================================= */

static int builtin_whoami(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "whoami: too many arguments\n");
        return -1;
    }

    struct passwd *pw = getpwuid(geteuid());
    if (pw != NULL)
        printf("%s\n", pw->pw_name);
    else
        printf("%d\n", (int)geteuid());

    return 0;
}


/* =========================================================
   BUILTIN: date            (NEW)
   ---------------------------------------------------------
   Prints the current local date and time in human-readable
   form. Implemented in the shell itself rather than calling
   the external /bin/date so it stays available on minimal
   systems.
   ========================================================= */

static int builtin_date(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "date: too many arguments\n");
        return -1;
    }

    time_t     now = time(NULL);
    struct tm  tm;

    if (localtime_r(&now, &tm) == NULL)
    {
        perror("date");
        return -1;
    }

    char buf[64];
    if (strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Z %Y", &tm) == 0)
    {
        fprintf(stderr, "date: strftime failed\n");
        return -1;
    }

    printf("%s\n", buf);
    return 0;
}


/* =========================================================
   BUILTIN: help            (NEW)
   ---------------------------------------------------------
   Built-in help system: lists every command Shellforge
   supports (builtins + features the shell provides) so the
   user does not have to look at the source.
   ========================================================= */

static int builtin_help(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "help: too many arguments\n");
        return -1;
    }

    printf("\n");
    printf("=========================================================\n");
    printf("            Shellforge - Built-in Help\n");
    printf("=========================================================\n");
    printf("\n");
    printf("Built-in commands:\n");
    printf("  cd <dir>        - change the working directory\n");
    printf("  pwd             - print the working directory\n");
    printf("  echo <text...>  - print its arguments\n");
    printf("  clear           - clear the terminal screen\n");
    printf("  whoami          - print the current user name\n");
    printf("  date            - print the current date and time\n");
    printf("  history         - show the command history\n");
    printf("  jobs            - list background / stopped jobs\n");
    printf("  fg <id>         - bring a job to the foreground\n");
    printf("  bg <id>         - resume a stopped job in the background\n");
    printf("  exit            - leave Shellforge\n");
    printf("\n");
    printf("Shell features:\n");
    printf("  < > >>          - input and output redirection\n");
    printf("  |               - pipelines\n");
    printf("  &               - run the previous command in background\n");
    printf("  $VAR            - environment variable expansion\n");
    printf("  '...'  \"...\"    - single and double quoted strings\n");
    printf("  Up / Down       - browse the command history\n");
    printf("\n");
    printf("=========================================================\n");
    printf("\n");

    return 0;
}


/* =========================================================
   CHECK WHETHER COMMAND IS A BUILTIN
   ========================================================= */

int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return 0;

    if (strcmp(cmd->argv[0], "cd")     == 0) return 1;
    if (strcmp(cmd->argv[0], "pwd")    == 0) return 1;
    if (strcmp(cmd->argv[0], "echo")   == 0) return 1;
    if (strcmp(cmd->argv[0], "exit")   == 0) return 1;
    if (strcmp(cmd->argv[0], "clear")  == 0) return 1;
    if (strcmp(cmd->argv[0], "whoami") == 0) return 1;
    if (strcmp(cmd->argv[0], "date")   == 0) return 1;
    if (strcmp(cmd->argv[0], "help")   == 0) return 1;
    if (strcmp(cmd->argv[0], "jobs")   == 0) return 1;
    if (strcmp(cmd->argv[0], "fg")     == 0) return 1;
    if (strcmp(cmd->argv[0], "bg")     == 0) return 1;

    return 0;
}


/* =========================================================
   EXECUTE BUILTIN
   ========================================================= */

int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return -1;

    if (strcmp(cmd->argv[0], "cd")     == 0) return builtin_cd(cmd);
    if (strcmp(cmd->argv[0], "pwd")    == 0) return builtin_pwd(cmd);
    if (strcmp(cmd->argv[0], "echo")   == 0) return builtin_echo(cmd);
    if (strcmp(cmd->argv[0], "exit")   == 0) return builtin_exit(cmd);
    if (strcmp(cmd->argv[0], "clear")  == 0) return builtin_clear(cmd);
    if (strcmp(cmd->argv[0], "whoami") == 0) return builtin_whoami(cmd);
    if (strcmp(cmd->argv[0], "date")   == 0) return builtin_date(cmd);
    if (strcmp(cmd->argv[0], "help")   == 0) return builtin_help(cmd);
    if (strcmp(cmd->argv[0], "jobs")   == 0) return builtin_jobs(cmd);
    if (strcmp(cmd->argv[0], "fg")     == 0) return builtin_fg(cmd);
    if (strcmp(cmd->argv[0], "bg")     == 0) return builtin_bg(cmd);

    return -1;
}
