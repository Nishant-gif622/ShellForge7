#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/history.h>
#include <readline/readline.h>

#include "history.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"
#include "jobs.h"

int main(void)
{
    /* Banner */
    printf("=====================================\n");
    printf("      Shellforge                     \n");
    printf(" A Unix Style Shell written in C     \n");
    printf(" Milestone 5 - with job control      \n");
    printf("=====================================\n");

    /* The shell runs in its own process group so that
       ^C / ^Z sent at the prompt stay on the shell. */
    job_set_shell_pgid(getpgrp());

    setup_background_handler();
    jobs_init();

    using_history();

    token_list_t  tokens;
    pipeline_t    pipeline;
    char         *line;

    while (1)
    {
        /* Clean up finished children before showing a prompt
           so they don't sit in the jobs table forever. */
        reap_jobs();

        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        if (strcmp(line, "history") == 0)
        {
            print_history();
            free(line);
            continue;
        }

        add_history(line);

        lexer(line, &tokens);

        if (parser(&tokens, &pipeline))
        {
            expand_variables(&pipeline);
        }

        /* 'exit' handled as builtin in the executor */
        if (pipeline.command_count == 1 &&
            pipeline.commands[0].argc > 0 &&
            strcmp(pipeline.commands[0].argv[0], "exit") == 0)
        {
            free(line);
            break;
        }

        execute_pipeline(&pipeline);

        free(line);
    }

    return 0;
}
