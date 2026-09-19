#!/bin/sh
# Reviewed source-only equivalent of the former TinyCC adapter.
set -eu
patch --batch --fuzz=0 -p1 -d "$1" <<'LEONOS_PATCH'
--- a/include/tccdefs.h
+++ b/include/tccdefs.h
@@ -336,3 +336,4 @@
     #undef __BUILTIN_EXTERN

     #endif /* ndef __TCC_PP__ */
+    #include <leonos_tccdefs.h>
--- a/tcc.c
+++ b/tcc.c
@@ -286,6 +286,19 @@
 #endif
 }

+static void leonos_write_help(const char *text)
+{
+    size_t remaining = strlen(text);
+    while (remaining) {
+        long written = write(1, text, remaining);
+        if (written <= 0) {
+            return;
+        }
+        text += written;
+        remaining -= (size_t)written;
+    }
+}
+
 int main(int argc, char **argv)
 {
     TCCState *s, *s1;
@@ -296,11 +309,29 @@
     char **argv0 = argv;
     FILE *ppfp = NULL;

+    /*
+     * No-input invocation only needs the usage text. Avoid creating a
+     * complete compiler state here: that state is unnecessary for help
+     * and its allocator teardown makes this fast path needlessly fragile
+     * on a small user process.
+     */
+    if (argc == 1) {
+        leonos_write_help(help);
+        return 0;
+    }
+
 redo:
     argc = argc0, argv = argv0;
     s = s1 = tcc_new();
     opt = tcc_parse_args(s, &argc, &argv);

+    if (s->output_type == TCC_OUTPUT_MEMORY) {
+        tcc_error_noabort("-run is not supported on LeonOS");
+    } else if (s->output_type == TCC_OUTPUT_DLL ||
+               (s->output_type & TCC_OUTPUT_DYN)) {
+        tcc_error_noabort("dynamic linking is not supported on LeonOS");
+    }
+
     if (n == 0) {
         ret = 0;
         if (opt == OPT_HELP) {
--- a/tcc.h
+++ b/tcc.h
@@ -133,6 +133,7 @@
 # define PATHSEP ";"
 #else
 # define IS_DIRSEP(c) (c == '/')
+/* LeonOS uses Unix-style absolute paths, for example /usr/lib/leonos. */
 # define IS_ABSPATH(p) IS_DIRSEP(p[0])
 # define PATHCMP strcmp
 # define PATHSEP ":"
@@ -201,6 +202,11 @@
 # endif
 #endif

+/* TCC is hosted by LeonOS but cannot execute generated memory images. */
+#ifdef LEONOS_TCC_PORT
+# undef TCC_IS_NATIVE
+#endif
+
 #if defined CONFIG_TCC_BACKTRACE && CONFIG_TCC_BACKTRACE==0
 # undef CONFIG_TCC_BACKTRACE
 #else
@@ -219,7 +225,9 @@
 #  define CONFIG_NEW_MACHO 1 /* enable new macho code */
 #endif

-#if defined TARGETOS_OpenBSD \
+#if defined LEONOS_TCC_TARGET
+# define TARGETOS_LeonOS 1
+#elif defined TARGETOS_OpenBSD \
     || defined TARGETOS_FreeBSD \
     || defined TARGETOS_NetBSD \
     || defined TARGETOS_FreeBSD_kernel
--- a/tccelf.c
+++ b/tccelf.c
@@ -1760,60 +1760,17 @@
 /* add libc crt1/crti objects */
 ST_FUNC void tccelf_add_crtbegin(TCCState *s1)
 {
-#if TARGETOS_OpenBSD
-    if (s1->output_type != TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crt0.o");
-    if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtbeginS.o");
-    else
-        tcc_add_crt(s1, "crtbegin.o");
-#elif TARGETOS_FreeBSD || TARGETOS_NetBSD
-    if (s1->output_type != TCC_OUTPUT_DLL)
-#if TARGETOS_FreeBSD
-        tcc_add_crt(s1, "crt1.o");
-#else
-        tcc_add_crt(s1, "crt0.o");
-#endif
-    tcc_add_crt(s1, "crti.o");
-    if (s1->static_link)
-        tcc_add_crt(s1, "crtbeginT.o");
-    else if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtbeginS.o");
-    else
-        tcc_add_crt(s1, "crtbegin.o");
-#elif TARGETOS_ANDROID
-    if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtbegin_so.o");
-    else
-        tcc_add_crt(s1, "crtbegin_dynamic.o");
-#else
-    if (s1->output_type != TCC_OUTPUT_DLL)
+    if (s1->output_type != TCC_OUTPUT_DLL) {
         tcc_add_crt(s1, "crt1.o");
-    tcc_add_crt(s1, "crti.o");
-#endif
+        tcc_add_crt(s1, "crti.o");
+        tcc_add_crt(s1, "mimalloc.o");
+    }
 }

 ST_FUNC void tccelf_add_crtend(TCCState *s1)
 {
-#if TARGETOS_OpenBSD
-    if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtendS.o");
-    else
-        tcc_add_crt(s1, "crtend.o");
-#elif TARGETOS_FreeBSD || TARGETOS_NetBSD
-    if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtendS.o");
-    else
-        tcc_add_crt(s1, "crtend.o");
-    tcc_add_crt(s1, "crtn.o");
-#elif TARGETOS_ANDROID
-    if (s1->output_type == TCC_OUTPUT_DLL)
-        tcc_add_crt(s1, "crtend_so.o");
-    else
-        tcc_add_crt(s1, "crtend_android.o");
-#else
-    tcc_add_crt(s1, "crtn.o");
-#endif
+    if (s1->output_type != TCC_OUTPUT_DLL)
+        tcc_add_crt(s1, "crtn.o");
 }
 #endif /* TCC_TARGET_UNIX */

@@ -1853,7 +1810,24 @@
 #endif
         if (lpthread)
             tcc_add_library(s1, "pthread");
+#if defined LEONOS_TCC_TARGET
+        /*
+         * This is the LeonOS static target link specification. Archives
+         * are rescanned as a group because the LeonOS adapter, musl,
+         * mbedTLS, and target runtime have recursive references. TinyCC
+         * resolves an archive only when tcc_add_library() is called, so
+         * one pass is not sufficient for all cross-archive dependencies.
+         */
+        tcc_add_support(s1, "libleonos-tcc-rt.a");
+        tcc_add_library(s1, "c");
+        tcc_add_library(s1, "leonos");
         tcc_add_library(s1, "c");
+        tcc_add_library(s1, "leonos");
+        tcc_add_library(s1, "c");
+        tcc_add_library(s1, "leonos");
+#else
+        tcc_add_library(s1, "c");
+#endif
 #ifdef TCC_LIBGCC
         if (!s1->static_link) {
             if (TCC_LIBGCC[0] == '/')
--- a/tccpp.c
+++ b/tccpp.c
@@ -3551,6 +3551,8 @@
 #else
 # if defined TCC_TARGET_MACHO
     "__APPLE__\0"
+# elif TARGETOS_LeonOS
+    "__leonos__\0"
 # elif TARGETOS_FreeBSD
     "__FreeBSD__ 12\0"
 # elif TARGETOS_FreeBSD_kernel
--- a/x86_64-link.c
+++ b/x86_64-link.c
@@ -67,6 +67,9 @@
 ST_FUNC int gotplt_entry_type (int reloc_type)
 {
     switch (reloc_type) {
+        /* TLS relaxation leaves a consumed relocation marker. */
+        case R_X86_64_NONE:
+            return NO_GOTPLT_ENTRY;
         case R_X86_64_GLOB_DAT:
         case R_X86_64_JUMP_SLOT:
         case R_X86_64_COPY:
--- /dev/null
+++ b/config.h
@@ -0,0 +1,16 @@
+/* Generated by the LeonOS Make adapter for LeonOS. */
+#define TCC_VERSION "0.9.28rc-leonos"
+#define TCC_TARGET_X86_64 1
+#define LEONOS_TCC_TARGET 1
+#define CONFIG_TCC_STATIC 1
+#define CONFIG_TCC_BACKTRACE 0
+#define CONFIG_TCC_BCHECK 0
+/* LeonOS runs a single compiler invocation per process. */
+#define CONFIG_TCC_SEMLOCK 0
+#define CONFIG_TCC_PREDEFS 0
+#define CONFIG_TCCDIR "/opt/tcc"
+#define CONFIG_TCC_SWITCHES "-static"
+#define CONFIG_TCC_SYSINCLUDEPATHS "{B}/include"
+#define CONFIG_TCC_LIBPATHS "{B}/lib"
+#define CONFIG_TCC_CRTPREFIX "{B}/lib"
+#define CONFIG_TCC_ELFINTERP ""
LEONOS_PATCH
