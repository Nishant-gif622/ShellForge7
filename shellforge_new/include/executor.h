#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

/* For tcsetpgrp() in jobs.c */
#include <sys/types.h>
#include <unistd.h>

int execute_command(command_t *cmd);

int execute_pipeline(pipeline_t *pipeline);

void reap_background_processes(void);

void setup_background_handler(void);

/* Job-control helpers implemented in jobs.c */
void job_set_shell_pgid(pid_t p);
pid_t job_get_shell_pgid(void);

#endif
