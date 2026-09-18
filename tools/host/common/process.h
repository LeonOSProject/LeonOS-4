/*
 * Subprocess execution for host build tools.
 */
#ifndef LEONOS_HOST_COMMON_PROCESS_H
#define LEONOS_HOST_COMMON_PROCESS_H

/**
 * @brief Run one program to completion using argv, never a shell string.
 *
 * The child stays in the caller's process group on purpose: Ctrl-C then reaches
 * it through the normal terminal signal path, and the tool does not have to
 * invent its own cleanup that could also hit unrelated build processes.
 *
 * @param program Executable to run. Resolved relative to the caller's cwd, not
 *                PATH, so callers must pass an absolute or explicit path.
 * @param argv NULL-terminated argument vector; argv[0] is the program name the
 *             child sees. Not modified.
 * @param working_dir Directory to run in, or NULL to inherit the caller's cwd.
 * @param out_child_status Receives the raw waitpid() status; inspect it with
 *                         WIFEXITED/WEXITSTATUS or WIFSIGNALED/WTERMSIG. May be
 *                         NULL when the caller only cares about launch success.
 * @return 0 when the child was launched and reaped (its exit status is reported
 *         through `out_child_status`, and a non-zero exit or a signal is NOT an
 *         error here), -1 when launch or reaping failed, with errno set.
 */
int run_process(const char *program, char *const argv[], const char *working_dir,
        int *out_child_status);

#endif /* LEONOS_HOST_COMMON_PROCESS_H */
