#!/bin/sh
# Exercise the real include logic without building a kernel or Kconfig frontend.
set -eu
src=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT HUP INT TERM
cat > "$w/Makefile" <<'MAKE'
include $(LEONOS_SRC)/mk/config.mk
.PHONY: check
check: $(LEONOS_AUTOCONF_MK)
	@test '$(KCONFIG_CONFIG_VMDK_DEFAULT_LANG)' = zh_CN.UTF-8
MAKE
mkdir -p "$w/include/generated"
printf 'CONFIG_VMDK_DEFAULT_LANG="zh_CN.UTF-8"\n' > "$w/config"
cat > "$w/config-tool" <<'SH'
#!/bin/sh
while [ "$#" -gt 0 ]; do
    case $1 in --out-header|--out-installer-header) touch "$2";; esac
    if [ "$1" = --make-include ]; then
        if [ ! -f "$2" ]; then
            printf 'KCONFIG_CONFIG_VMDK_DEFAULT_LANG :=zh_CN.UTF-8\n' > "$2"
        fi
        exit 0
    fi
    shift
done
exit 1
SH
chmod +x "$w/config-tool"
make --no-print-directory -f "$w/Makefile" check LEONOS_SRC="$src" \
    O_CONFIG="$w" O_INCLUDE="$w/include" O_HOST="$w/host" \
    LEONOS_CONFIG_FILE="$w/config" KCONFIG_CONF= KCONFIG_SEED= KCONFIG_ROOT= \
    LEONOS_CONFIG_TOOL="$w/config-tool"
printf 'first build: generated configuration loaded before recipes passed\n'
