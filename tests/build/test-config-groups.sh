#!/bin/sh
# Exercise the production fragment with counting transforms under parallel make.
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
w=$(mktemp -d)
trap 'rm -rf "$w"' EXIT
mkdir -p "$w/src/configs" "$w/src/tools/build" "$w/out/config" "$w/out/host/kconfig-frontends/bin" "$w/out/generated"
touch "$w/src/configs/default.conf" "$w/src/Kconfig" "$w/src/Kconfig.components" "$w/src/puff.c"
cat > "$w/src/tools/build/kconfig-frontends.sh" <<'SH'
set -eu
printf 'build\n' >> "$TEST_WORK/frontend-count"
mkdir -p "$TEST_WORK/out/host/kconfig-frontends/bin"
touch "$TEST_WORK/out/host/kconfig-frontends/bin/kconfig-conf" "$TEST_WORK/out/host/kconfig-frontends/bin/kconfig-mconf"
SH
cat > "$w/config-tool" <<'SH'
#!/bin/sh
set -eu
printf 'generate\n' >> "$TEST_WORK/config-count"
while [ "$#" -gt 0 ]; do
 case "$1" in --out-header|--out-installer-header|--make-include) shift; touch "$1";; esac
 shift
done
SH
chmod +x "$w/config-tool"
cat > "$w/test.mk" <<'MAKE'
LEONOS_SRC := $(TEST_WORK)/src
O_CONFIG := $(TEST_WORK)/out/config
O_HOST := $(TEST_WORK)/out/host
O_INCLUDE := $(TEST_WORK)/out/include
O_GENERATED := $(TEST_WORK)/out/generated
LEONOS_HOST_PUFF_SRC := puff.c
LEONOS_CONFIG_TOOL := $(TEST_WORK)/config-tool
LEONOS_PASSIVE := 1
include $(TEST_REPO)/mk/config.mk
.PHONY: front derived
front: $(KCONFIG_CONF) $(KCONFIG_MCONF)
derived: $(AUTOCONF_H) $(AUTOCONF_INSTALLER_H) $(LEONOS_AUTOCONF_MK)
$(O_INCLUDE)/generated:
	@mkdir -p $@
MAKE
export TEST_WORK="$w" TEST_REPO="$repo"
make -s -j8 -f "$w/test.mk" front
fail=0
if [ "$(wc -l < "$w/frontend-count")" -eq 1 ]; then echo 'ok: frontend runs once'; else echo 'FAIL: frontend executed multiple times'; fail=1; fi
touch "$w/out/config/.config"
make -s -j8 -f "$w/test.mk" derived
if [ "$(wc -l < "$w/config-count")" -eq 1 ]; then echo 'ok: config transform runs once'; else echo 'FAIL: config transform executed multiple times'; fail=1; fi
rm "$w/out/config/autoconf.mk"
make -s -j8 -f "$w/test.mk" derived
if [ -f "$w/out/config/autoconf.mk" ] && [ "$(wc -l < "$w/config-count")" -eq 2 ]; then echo 'ok: missing grouped output regenerated'; else echo 'FAIL: missing grouped output not regenerated once'; fail=1; fi
exit "$fail"
