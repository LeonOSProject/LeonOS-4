#!/bin/sh
# LeonOS adapters for upstream C programs. No upstream source is modified.
set -eu
MAKEFLAGS=${MAKEFLAGS-}
case ${MAKEFLAGS%% *} in *n*) exit 0 ;; esac
[ "$#" = 12 ] || { echo 'usage: upstream-app.sh PACKAGE SRC WORK OUTPUT MUSL RUNTIME ARCHIVE INCLUDE AUTOCONF CC LD AR' >&2; exit 2; }
pkg=$1 src=$2 work=$3 output=$4 musl=$5 runtime=$6 archive=$7 includes=$8 autoconf=$9
shift 9
cc=$1 ld=$2 ar=$3
resource=$("$cc" -print-resource-dir)
mkdir -p "$work" "$output"
rm -rf "$work/objects" "$work/generated"
mkdir -p "$work/objects" "$work/generated"
source=$src/third_party/$pkg
port=$src/userland/$pkg
source_id=$pkg
case $pkg in tcc) source_id=tinycc ;; pleditor) source_id=pl_editor ;; esac
if [ -n "${UPSTREAM_DEPS-}" ]; then
 expected=$("$UPSTREAM_DEPS" --lock "$src/configs/dependencies.lock.json" --id "$source_id" --print commit)
 [ "$(git -C "$src/third_party/$source_id" rev-parse HEAD)" = "$expected" ] || { echo "$pkg: source revision mismatch" >&2; exit 1; }
fi
flags="--target=x86_64-linux-musl ${UPSTREAM_CFLAGS:--O2} -std=gnu11 -ffreestanding -fno-stack-protector -fPIC -ffunction-sections -fdata-sections -nostdinc -isystem $resource/include -I$musl/include -I$src/userland/libc/include -I$src/include -I$src/include/uapi -I$includes -I$work/generated -I$source -DLEONOS_USE_MUSL -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L"
compile() { "$cc" $flags "$@"; }
shared() { name=$1; shift; "$ld" ${UPSTREAM_LDFLAGS-} -shared --no-undefined --hash-style=both -z max-page-size=0x1000 -soname "$name" -o "$output/$name.tmp" "$@" -L "$musl/lib" -l:libmimalloc.so.3 "$runtime" -lc; mv "$output/$name.tmp" "$output/$name"; }
staticlib() { name=$1; shift; rm -f "$output/$name.tmp"; "$ar" rcs "$output/$name.tmp" "$@"; mv "$output/$name.tmp" "$output/$name"; }
executable() { "$ld" ${UPSTREAM_LDFLAGS-} --gc-sections -z max-page-size=0x1000 -pie --hash-style=both --dynamic-linker /lib/ld-musl-x86_64.so.1 -rpath /usr/lib/leonos:/lib:/usr/lib -o "$output/$pkg.elf.tmp" "$musl/lib/Scrt1.o" "$musl/lib/crti.o" "$@" -L "$musl/lib" -l:libmimalloc.so.3 --start-group "$runtime" -lc --end-group "$musl/lib/crtn.o"; mv "$output/$pkg.elf.tmp" "$output/$pkg.elf"; }
staticexe() { "$ld" ${UPSTREAM_LDFLAGS-} --gc-sections -z max-page-size=0x1000 -static --image-base=0x4000000 -o "$output/$pkg.elf.tmp" "$musl/lib/crt1.o" "$musl/lib/crti.o" "$@" "$musl/lib/mimalloc.o" --start-group "$archive" "$musl/lib/libc.a" --end-group "$musl/lib/crtn.o"; mv "$output/$pkg.elf.tmp" "$output/$pkg.elf"; }
case $pkg in
cmd)
 cp -R "$source" "$work/generated/source"
 source=$work/generated/source
 sh "$src/tools/build/upstream-cmd-patch.sh" "$source"
 flags="$flags -std=c99 -fno-pic -fno-pie -D_DEFAULT_SOURCE -I$port/include -I$source -I$port"
 for path in "$source"/*.c; do
  name=${path##*/}
  case $name in lexec.c|llinenoise.c|lpath.c|lreadline.c|lsysport.c) continue ;; esac
  compile -c "$path" -o "$work/objects/${name%.c}.o"
 done
 sed 's/char node_name\[_UTSNAME_LENGTH\];/char node_name[sizeof(name->nodename)];/' "$port/leonos_cmd_shim.c" > "$work/generated/shim.c"
 compile -Ustat -Ufstat -Ulstat -c "$work/generated/shim.c" -o "$work/objects/shim.o"
 staticexe "$work"/objects/*.o
 ;;
tcc)
 cp -R "$src/third_party/tinycc" "$work/generated/source"
 source=$work/generated/source
 sh "$src/tools/build/upstream-tcc-patch.sh" "$source"
 cp "$port/leonos_tccdefs.h" "$source/include/"
 flags="$flags -std=c11 -fno-pic -fno-pie -DLEONOS_TCC_PORT=1 -I$source -I$src/devtools/include"
 compile -DONE_SOURCE=1 -c "$source/tcc.c" -o "$work/tcc.o"
 compile -c "$port/leonos_tcc_float_runtime.c" -o "$work/float.o"
 for name in libtcc1.c stdatomic.c builtin.c atomic.S alloca.S alloca-bt.S va_list.c dsohandle.c; do compile -I"$source/lib" -c "$source/lib/$name" -o "$work/objects/${name%.*}.o"; done
 staticexe "$work/tcc.o" "$work/float.o"
 stage=$output/tcc-runtime.new
 rm -rf "$stage"
 mkdir -p "$stage/lib" "$stage/include"
 cp -a "$musl/include/." "$stage/include/"
 cp -an "$src/devtools/include/." "$stage/include/"
 cp -a "$src/include/uapi/." "$stage/include/"
 cp "$source"/include/* "$stage/include/"
 cp "$source/tcclib.h" "$src/third_party/zlib/zlib.h" "$src/third_party/zlib/zconf.h" "$src/third_party/libpng/png.h" "$src/third_party/libpng/pngconf.h" "$includes/libpng/pnglibconf.h" "$stage/include/"
 "$ar" rcs "$stage/lib/libtcc1.a" "$work"/objects/*.o
 "$ar" rcs "$stage/lib/libleonos-tcc-rt.a" "$work/float.o"
 cp "$archive" "$stage/lib/libleonos.a"
 cp "$musl/lib/libc.a" "$musl/lib/crt1.o" "$musl/lib/crti.o" "$musl/lib/crtn.o" "$musl/lib/mimalloc.o" "$stage/lib/"
 cp "$work/objects/alloca.o" "$stage/lib/"
 # Preserve separate SDK archives; the unified runtime has duplicate basename
 # members (notably png.c.o), so extracting by archive member name is unsafe.
 runtime_objects=${UPSTREAM_RUNTIME_OBJECTS:-${runtime%/system/lib/libleonos.so.2}/obj/runtime/third_party}
 "$ar" rcs "$stage/lib/libz.a" "$runtime_objects"/zlib/*.o
 "$ar" rcs "$stage/lib/libpng.a" "$runtime_objects"/libpng/*.o
 cp -a "$musl/share/licenses" "$stage/licenses"
 cp "$source/COPYING" "$stage/COPYING"
 cp "$port/README.md" "$stage/README-LEONOS.md"
 cp -a "$port/examples" "$stage/examples"
 rm -rf "$output/tcc-runtime.previous"
 if [ -e "$output/tcc-runtime" ]; then mv "$output/tcc-runtime" "$output/tcc-runtime.previous"; fi
 mv "$stage" "$output/tcc-runtime"
 rm -rf "$output/tcc-runtime.previous"
 ;;
pleditor)
 source=$src/third_party/pl_editor
 cp -R "$source/src" "$work/generated/src"
 flags="$flags -I$work/generated/src"
 awk '
 /#include <stdlib.h>/ {print; print "#include <stdint.h>"; next}
 /    size_t buffer_size = \(size_t\)\(state->screen_rows \+ 2\) \*/ {
   print "    size_t rows = (size_t)state->screen_rows;"
   print "    size_t cols = (size_t)state->screen_cols;"
   print "    if (state->screen_rows < 0 || state->screen_cols < 0 || rows > SIZE_MAX - 2U || cols > (SIZE_MAX - 32U) / 16U) return;"
   print "    size_t bytes_per_row = cols * 16U + 32U;"
   print "    rows += 2U;"
   print "    if (rows > (SIZE_MAX - 256U) / bytes_per_row) return;"
   print "    size_t buffer_capacity = rows * bytes_per_row + 256U;"
   print "    char *buffer = malloc(buffer_capacity);"
   getline; getline; changed++; next
 }
 {print}
 END {if (changed != 1) exit 1}
 ' "$source/src/pleditor.c" > "$work/generated/src/pleditor.c"
 for name in main pleditor syntax; do compile -include "$autoconf" -fPIE -c "$work/generated/src/$name.c" -o "$work/objects/$name.o"; done
 compile -include "$autoconf" -fPIE -c "$src/userland/apps/pleditor/platform_leonos.c" -o "$work/objects/platform.o"
 executable "$work"/objects/*.o
 ;;
lua)
 names='lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit liolib llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio'
 for name in $names; do compile -DLUA_USE_C89 '-DLUA_PATH_DEFAULT="/opt/lua/lua/?.lua;/opt/lua/lua/?/init.lua;/usr/share/lua/5.4/?.lua;./?.lua;./?/init.lua"' '-DLUA_CPATH_DEFAULT="/usr/lib/lua/5.4/?.so"' -c "$source/$name.c" -o "$work/objects/$name.o"; done
 shared liblua.so.5 "$work"/objects/*.o
 staticlib liblua.a "$work"/objects/*.o
 compile -DLUA_USE_C89 -fPIE -c "$source/lua.c" -o "$work/lua.o"
 executable "$work/lua.o" "$output/liblua.so.5"
 ;;
sl)
 compile -D_DEFAULT_SOURCE -fPIE -c "$source/sl.c" -o "$work/objects/sl.o"
 executable "$work/objects/sl.o"
 ;;
less)
 flags="$flags -D_DEFAULT_SOURCE -I$port/include"
 names='main screen brac ch charset cmdbuf command cvt decode edit evar filename forwback ifile input jump line linenum lmsg lsystem mark optfunc option opttbl os output pattern position prompt search signal tags ttyin version xbuf'
 for name in $names; do sed -n '/^[[:space:]]*public /{/;/!{s/^[[:space:]]*//;s/$/;/;p;}}' "$source/$name.c"; done > "$work/generated/funcs.h"
 awk '/^[[:space:]]*#/ {next} NF { sub(/^[[:space:]]*/, ""); name=$1; sub(/^[^ ]+ +/, ""); print "M(" name ",\"" $0 "\")" }' "$source/lessmsg" "$source/lessmsg_int" > "$work/generated/lessmsg.inc"
 { printf '#include "less.h"\nconstant char helpdata[] = {\n'; od -An -v -tu1 "$source/less.hlp" | awk '{for(i=1;i<=NF;i++){c=$i;if(c==13){if(prev!=10)printf "10,"}else if(c==10){if(prev!=13)printf "10,"}else printf "%d,",c;prev=c}}'; printf '0};\nconstant int size_helpdata = sizeof(helpdata)-1;\n'; } > "$work/generated/help.c"
 for name in $names; do compile -fPIE -c "$source/$name.c" -o "$work/objects/$name.o"; done
 compile -c "$work/generated/help.c" -o "$work/objects/help.o"
 compile -c "$port/leonos_termcap.c" -o "$work/objects/leonos_termcap.o"
 executable "$work"/objects/*.o
 ;;
file)
 cp -R "$source/src" "$work/generated/src"
 sed -i 's/#if defined(__EMX__) || defined (WIN32)/#if defined(__EMX__) || defined (WIN32) || defined(LEONOS_FILE_PATHSEP_SEMICOLON)/' "$work/generated/src/file.h"
 sed 's/X.YY/548/g' "$source/src/magic.h.in" > "$work/generated/magic.h"
 cp "$port/config.h" "$work/generated/config.h"
 flags="$flags -I$work/generated/src -DHAVE_CONFIG_H -DLEONOS_FILE_PATHSEP_SEMICOLON"
 names='buffer magic apprentice softmagic ascmagic encoding is_csv is_json is_simh is_tar readelf print fsmagic funcs apptype der cdf cdf_time readcdf swap asprintf vasprintf dprintf getline strcasestr strlcat strlcpy ctime_r asctime_r gmtime_r localtime_r fmtcheck'
 for name in $names; do compile '-DMAGIC="/usr/share/misc/magic.mgc"' -c "$work/generated/src/$name.c" -o "$work/objects/$name.o"; done
 compile -c "$port/leonos_shim.c" -o "$work/objects/leonos_shim.o"
 shared libmagic.so.1 "$work"/objects/*.o
 staticlib libmagic.a "$work"/objects/*.o
 compile '-DMAGIC="/usr/share/misc/magic.mgc"' -c "$work/generated/src/file.c" -o "$work/file.o"
 compile -c "$work/generated/src/seccomp.c" -o "$work/seccomp.o"
 executable "$work/file.o" "$work/seccomp.o" "$output/libmagic.so.1"
 cp "$work/generated/magic.h" "$output/magic.h"
 ;;
sqlite)
 sed "s@TOP = ../sqlite@TOP = $source@" "$source/Makefile.linux-gcc" > "$work/generated/Makefile"
 (cd "$work/generated" && make sqlite3.c sqlite3.h)
 flags="$flags -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION=1 -DSQLITE_OMIT_WAL=1 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_MAX_MMAP_SIZE=0"
 compile -c "$work/generated/sqlite3.c" -o "$work/objects/sqlite3.o"
 compile -c "$port/leonos_sqlite_vfs.c" -o "$work/objects/vfs.o"
 shared sqlite.so.3 "$work"/objects/*.o
 staticlib libsqlite3.a "$work"/objects/*.o
 cp "$work/generated/sqlite3.h" "$output/sqlite3.h"
 ;;
*) echo "unsupported upstream app: $pkg" >&2; exit 2 ;;
esac
if [ "$pkg" = tcc ]; then
 (cd "$output" && find tcc-runtime \( -type f -o -type l \) -print | LC_ALL=C sort) > "$work/installed-files"
fi
