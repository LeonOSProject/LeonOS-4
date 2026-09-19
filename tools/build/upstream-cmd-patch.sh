#!/bin/sh
# Reviewed source-only equivalent of the former cmd adapter.
set -eu
patch --batch --fuzz=0 -p1 -d "$1" <<'LEONOS_PATCH'
--- a/cinterp.c
+++ b/cinterp.c
@@ -782,8 +782,6 @@
     while (!ctx->should_exit)
     {
         char *result;
-        char *prompt = NULL;
-        size_t prompt_len = 0;

         /* Ctrl+C during command execution: the terminal already echoed
          * '^C'; move to a fresh line before showing the next prompt */
@@ -798,16 +796,12 @@
             }
         }

-        /* Render the prompt and hand it to the line editor, so it knows where
-         * the input area starts (fixes backspace eating the prompt) */
+        /* LeonOS exposes canonical PTY input. Render the prompt directly and
+         * let the small port reader consume the completed line. */
         if (is_tty)
         {
-            FILE *m = libcmd_open_memstream(&prompt, &prompt_len);
-            if (m)
-            {
-                render_prompt(ctx, m);
-                libcmd_memstream_close(m);
-            }
+            render_prompt(ctx, stdout);
+            fflush(stdout);
         }

         /* Catch Ctrl+Z only while readline is editing: it is echoed as
@@ -824,7 +818,7 @@
             sigemptyset(&sa_tstp.sa_mask);
             sa_tstp.sa_flags = 0;
             sigaction(SIGTSTP, &sa_tstp, &sa_prev);
-            result = libcmd_readline(is_tty && prompt ? prompt : "", line, sizeof(line));
+            result = libcmd_readline("", line, sizeof(line));
             sigaction(SIGTSTP, &sa_prev, NULL);
         }
         else
@@ -832,8 +826,6 @@
             result = libcmd_readline("", line, sizeof(line));
         }

-        free(prompt);
-
         if (result == NULL)
         {
             /* EOF, or interrupted by Ctrl+C */
--- a/cparser.c
+++ b/cparser.c
@@ -610,12 +610,29 @@
                        int stdin_fd, int stdout_fd, int stderr_fd);
 static int exec_node_fds(cmd_context_t *ctx, cmd_node_t *node,
                          int stdin_fd, int stdout_fd, int stderr_fd);
+static int leonos_start_background(cmd_node_t *node,
+                                   int stdin_fd, int stdout_fd, int stderr_fd);
+
+/* LeonOS background-job adapter backed by COW fork/waitpid. */
+extern int leonos_cmd_builtin(int argc, char **argv, int *handled);
+extern int leonos_cmd_register_job(const int pids[], int count, int last_pid,
+                                   const char *text);
+extern void leonos_cmd_job_append_word(char *out, size_t cap, const char *word);
+extern int leonos_cmd_set_process_group(int pid, int process_group);
+extern int leonos_cmd_foreground_enter(int fd, int process_group, int *saved_group);
+extern void leonos_cmd_foreground_leave(int fd, int saved_group);
+

 static int exec_simple(cmd_context_t *ctx, cmd_node_t *node,
                        int stdin_fd, int stdout_fd, int stderr_fd)
 {
+    int handled = 0;
+    int result;
     if (node == NULL || node->argc == 0)
         return 0;
+    result = leonos_cmd_builtin(node->argc, node->argv, &handled);
+    if (handled)
+        return result;
     return cmd_dispatch(ctx, node->argc, node->argv,
                         stdin_fd, stdout_fd, stderr_fd);
 }
@@ -767,6 +784,8 @@
         int n = collect_pipe_stages(node, stages, 64);
         int prev_read = stdin_fd;
         int pids[64];
+        int process_group = 0;
+        int saved_group = 0;
         int i;

         if (n <= 0)
@@ -830,6 +849,9 @@

             /* Parent */
             pids[i] = (int)pid;
+            if (process_group == 0)
+                process_group = (int)pid;
+            (void)leonos_cmd_set_process_group((int)pid, process_group);
             if (prev_read >= 0 && prev_read != stdin_fd)
                 libcmd_close(prev_read);
             if (i + 1 < n) {
@@ -840,11 +862,13 @@

         /* Wait for all children; the exit code is the last stage's */
         ret = 0;
+        (void)leonos_cmd_foreground_enter(stdin_fd, process_group, &saved_group);
         for (i = 0; i < n; i++) {
             libcmd_exit_info_t ei;
             if (libcmd_wait_pid(pids[i], &ei) == 0 && i == n - 1)
                 ret = ei.exit_code;
         }
+        leonos_cmd_foreground_leave(stdin_fd, saved_group);
         break;
     }

@@ -861,14 +885,85 @@
         break;

     case NODE_SEQ:
-        exec_node_fds(ctx, node->left, stdin_fd, stdout_fd, stderr_fd);
-        ret = exec_node_fds(ctx, node->right, stdin_fd, stdout_fd, stderr_fd);
+        ret = leonos_start_background(node->left, stdin_fd, stdout_fd, stderr_fd);
+        if (node->right)
+            ret = exec_node_fds(ctx, node->right, stdin_fd, stdout_fd, stderr_fd);
         break;
     }

     return ret;
 }

+static int leonos_start_background(cmd_node_t *node,
+                                   int stdin_fd, int stdout_fd, int stderr_fd)
+{
+    static char resolved[64][CMD_MAX_PATH];
+    cmd_node_t *stages[64];
+    char *const *argvs[64];
+    const char *paths[64];
+    int pids[64];
+    char text[160];
+    int n;
+    int i;
+    if (!node) return 0;
+    memset(text, 0, sizeof(text));
+    if (node->type == NODE_SIMPLE) {
+        if (node->argc == 0 || node->redirs || cmd_find_builtin(node->argv[0]) ||
+            libcmd_find_exec(node->argv[0], libcmd_getenv("PATH"), resolved[0],
+                             sizeof(resolved[0])) < 0) {
+            fputs("cmd: background jobs require an external command without redirection\n", stderr);
+            return 1;
+        }
+        pids[0] = libcmd_exec_job_async(resolved[0], node->argv, libcmd_get_environ(),
+                                        stdin_fd, stdout_fd, stderr_fd, 0);
+        if (pids[0] < 0) {
+            fputs("cmd: unable to start background command\n", stderr);
+            return 1;
+        }
+        for (i = 0; i < node->argc; ++i)
+            leonos_cmd_job_append_word(text, sizeof(text), node->argv[i]);
+        if (leonos_cmd_register_job(pids, 1, pids[0], text) < 0) {
+            (void)kill(pids[0], SIGTERM);
+            fputs("cmd: job table is full\n", stderr);
+            return 1;
+        }
+        return 0;
+    }
+    if (node->type != NODE_PIPE) {
+        fputs("cmd: background jobs support external commands and pipelines only\n", stderr);
+        return 1;
+    }
+    n = collect_pipe_stages(node, stages, 64);
+    if (n <= 0) return 1;
+    for (i = 0; i < n; ++i) {
+        int j;
+        if (stages[i]->type != NODE_SIMPLE || stages[i]->argc == 0 || stages[i]->redirs ||
+            cmd_find_builtin(stages[i]->argv[0]) ||
+            libcmd_find_exec(stages[i]->argv[0], libcmd_getenv("PATH"), resolved[i],
+                             sizeof(resolved[i])) < 0) {
+            fputs("cmd: background pipelines require external commands without redirection\n", stderr);
+            return 1;
+        }
+        argvs[i] = stages[i]->argv;
+        paths[i] = resolved[i];
+        if (i) leonos_cmd_job_append_word(text, sizeof(text), "|");
+        for (j = 0; j < stages[i]->argc; ++j)
+            leonos_cmd_job_append_word(text, sizeof(text), stages[i]->argv[j]);
+    }
+    n = libcmd_exec_pipeline_async(argvs, paths, n, libcmd_get_environ(),
+                                   stdin_fd, stdout_fd, pids, 64);
+    if (n < 0) {
+        fputs("cmd: unable to start background pipeline\n", stderr);
+        return 1;
+    }
+    if (leonos_cmd_register_job(pids, n, pids[n - 1], text) < 0) {
+        for (i = 0; i < n; ++i) (void)kill(pids[i], SIGTERM);
+        fputs("cmd: job table is full\n", stderr);
+        return 1;
+    }
+    return 0;
+}
+
 /* Apply redirections and execute */
 int cmd_exec_node(cmd_context_t *ctx, cmd_node_t *node)
 {
--- a/cvars.c
+++ b/cvars.c
@@ -423,13 +423,16 @@

 char *cmd_expand_vars(cmd_context_t *ctx, const char *line)
 {
-    char  out[CMD_MAX_LINE * 4];
+    char *out;
     int   opos = 0;
-    int   olen = (int)(sizeof(out) - 1);
+    int   olen = CMD_MAX_LINE * 4 - 1;
     const char *p = line;

     if (line == NULL)
         return libcmd_strdup("");
+    out = (char *)malloc((size_t)CMD_MAX_LINE * 4U);
+    if (out == NULL)
+        return libcmd_strdup("");

 #define EMIT(c) do { if (opos < olen) out[opos++] = (c); } while(0)
 #define EMITS(s) do { \
@@ -603,6 +606,10 @@
 #undef EMIT
 #undef EMITS

+    char *result;
+
     out[opos] = '\0';
-    return libcmd_strdup(out);
+    result = libcmd_strdup(out);
+    free(out);
+    return result;
 }
--- a/glibcmd.h
+++ b/glibcmd.h
@@ -492,6 +492,28 @@
                          int stdout_fd,
                          libcmd_exit_info_t *exit_info);

+/* Starts an external pipeline through fork/exec without waiting. pids receives
+ * one child PID per stage and the return value is the number of started stages. */
+int libcmd_exec_pipeline_async(char *const *const *cmds,
+                               const char *const *paths,
+                               int n,
+                               char *const envp[],
+                               int stdin_fd,
+                               int stdout_fd,
+                               int pids[],
+                               int pids_capacity);
+
+/* Starts an external shell job without creating the detached session used by
+ * cmd.exe START. The child becomes the leader of its process group. */
+int libcmd_exec_job_async(const char *path,
+                          char *const argv[],
+                          char *const envp[],
+                          int stdin_fd,
+                          int stdout_fd,
+                          int stderr_fd,
+                          int nice_level);
+
+
 /*
  * libcmd_fork - raw fork(2) wrapper
  * Returns the child PID in the parent, 0 in the child, -1 on error.
--- a/lfs.c
+++ b/lfs.c
@@ -276,67 +276,39 @@
     if (de == NULL)
         return -1;

+    /* A LeonOS directory read already supplies the name and type.  Reset all
+     * optional fields before a best-effort stat so an error cannot reuse the
+     * preceding entry's metadata or suppress this entry from DIR output. */
+    memset(entry, 0, sizeof(*entry));
     strncpy(entry->name, de->d_name, LIBCMD_NAME_MAX - 1);
     entry->name[LIBCMD_NAME_MAX - 1] = '\0';
-
-    /* Use d_type when available, fall back to stat */
 #ifdef DT_DIR
-    if (de->d_type != DT_UNKNOWN) {
-        entry->is_dir  = (de->d_type == DT_DIR)  ? LIBCMD_TRUE : LIBCMD_FALSE;
-        entry->is_link = (de->d_type == DT_LNK)  ? LIBCMD_TRUE : LIBCMD_FALSE;
-        entry->size    = 0;
-        entry->mode    = 0;
-        /* We still need mtime; do a stat */
-    }
+    entry->is_dir = (de->d_type == DT_DIR) ? LIBCMD_TRUE : LIBCMD_FALSE;
+    entry->is_link = (de->d_type == DT_LNK) ? LIBCMD_TRUE : LIBCMD_FALSE;
 #endif

-    if (stat(de->d_name, &st) == 0) {
-        entry->is_dir  = S_ISDIR(st.st_mode) ? LIBCMD_TRUE : LIBCMD_FALSE;
-        entry->is_link = S_ISLNK(st.st_mode) ? LIBCMD_TRUE : LIBCMD_FALSE;
-        entry->size    = st.st_size;
-        entry->mode    = (unsigned int)st.st_mode;
-        entry->uid     = (unsigned int)st.st_uid;
-
-        t = st.st_mtime;
-        tm_info = localtime(&t);
-        if (tm_info) {
-            entry->mtime.year   = tm_info->tm_year + 1900;
-            entry->mtime.month  = tm_info->tm_mon  + 1;
-            entry->mtime.day    = tm_info->tm_mday;
-            entry->mtime.hour   = tm_info->tm_hour;
-            entry->mtime.minute = tm_info->tm_min;
-            entry->mtime.second = tm_info->tm_sec;
-            entry->mtime.ms     = 0;
-            entry->mtime.wday   = tm_info->tm_wday;
-        }
+    if (stat(de->d_name, &st) != 0)
+        return 0;

-        t = st.st_atime;
-        tm_info = localtime(&t);
-        if (tm_info) {
-            entry->atime.year   = tm_info->tm_year + 1900;
-            entry->atime.month  = tm_info->tm_mon  + 1;
-            entry->atime.day    = tm_info->tm_mday;
-            entry->atime.hour   = tm_info->tm_hour;
-            entry->atime.minute = tm_info->tm_min;
-            entry->atime.second = tm_info->tm_sec;
-            entry->atime.ms     = 0;
-            entry->atime.wday   = tm_info->tm_wday;
-        }
-
-        t = st.st_ctime;
-        tm_info = localtime(&t);
-        if (tm_info) {
-            entry->ctime.year   = tm_info->tm_year + 1900;
-            entry->ctime.month  = tm_info->tm_mon  + 1;
-            entry->ctime.day    = tm_info->tm_mday;
-            entry->ctime.hour   = tm_info->tm_hour;
-            entry->ctime.minute = tm_info->tm_min;
-            entry->ctime.second = tm_info->tm_sec;
-            entry->ctime.ms     = 0;
-            entry->ctime.wday   = tm_info->tm_wday;
-        }
+    entry->is_dir = S_ISDIR(st.st_mode) ? LIBCMD_TRUE : LIBCMD_FALSE;
+    entry->is_link = S_ISLNK(st.st_mode) ? LIBCMD_TRUE : LIBCMD_FALSE;
+    entry->size = st.st_size;
+    entry->mode = (unsigned int)st.st_mode;
+    entry->uid = (unsigned int)st.st_uid;
+
+    t = st.st_mtime;
+    tm_info = localtime(&t);
+    if (tm_info) {
+        entry->mtime.year = tm_info->tm_year + 1900;
+        entry->mtime.month = tm_info->tm_mon + 1;
+        entry->mtime.day = tm_info->tm_mday;
+        entry->mtime.hour = tm_info->tm_hour;
+        entry->mtime.minute = tm_info->tm_min;
+        entry->mtime.second = tm_info->tm_sec;
+        entry->mtime.wday = tm_info->tm_wday;
     }
-
+    entry->atime = entry->mtime;
+    entry->ctime = entry->mtime;
     return 0;
 }

LEONOS_PATCH
