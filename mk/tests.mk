# Test targets.
#
# The core harness is POSIX shell and C: the acceptance rule for this migration
# may not depend on the interpreter it is auditing (plan section 2).

LEONOS_TEST_TMP := $(if $(TMPDIR),$(TMPDIR),/tmp)/leonos-tests-$(shell id -u)

# C unit tests for the shared host primitives.
LEONOS_HOST_TEST_BINS := $(O_HOST)/tests/test_common

# Shell contract tests. test-bootstrap.sh is the public entry-point surface.
LEONOS_BUILD_TESTS := $(sort $(wildcard $(LEONOS_SRC)/tests/build/test-*.sh))

# The plain and sanitised suites are separate goals so `test-tools` keeps a
# single recipe: the plan requires ASan/UBSan at the tool boundary, not merely a
# clean compile.
.PHONY: leonos-test-tools-plain leonos-test-tools-sanitised

test-tools: leonos-test-tools-plain leonos-test-tools-sanitised

leonos-test-tools-plain: $(LEONOS_HOST_TEST_BINS)
	@set -eu; for test_binary in $(LEONOS_HOST_TEST_BINS); do \
	    printf '  %-8s %s\n' RUN $$test_binary; \
	    $$test_binary; \
	done

leonos-test-tools-sanitised: $(O_HOST)/tests-sanitised/test_common
	@set -eu; \
	    ASAN_OPTIONS=detect_leaks=1 \
	    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	    $(O_HOST)/tests-sanitised/test_common

test-build: $(LEONOS_HOST_TOOLS)
	@set -eu; for contract_test in $(LEONOS_BUILD_TESTS); do \
	    printf '  %-8s %s\n' RUN $$contract_test; \
	    sh $$contract_test; \
	done

test: test-tools test-build

$(O_HOST)/obj/tests/host/%.c.o: $(LEONOS_SRC)/tests/host/%.c $(O_META)/host-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' HOSTCC $<
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) -I$(LEONOS_SRC)/tests/host \
	    $(LEONOS_HOST_INCLUDES) $(HOST_CFLAGS) -MMD -MF $@.d -c $< -o $@

$(O_HOST)/obj/tests-sanitised/host/%.c.o: $(LEONOS_SRC)/tests/host/%.c $(O_META)/host-cc-sanitised.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)printf '  %-8s %s\n' HOSTCC $<
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) \
	    -I$(LEONOS_SRC)/tests/host $(LEONOS_HOST_INCLUDES) -g -O1 \
	    -MMD -MF $@.d -c $< -o $@

$(O_HOST)/obj/tests-sanitised/tools/host/common/%.c.o: $(LEONOS_SRC)/tools/host/common/%.c \
	$(O_META)/host-cc-sanitised.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) -g -O1 \
	    $(LEONOS_HOST_INCLUDES) -MMD -MF $@.d -c $< -o $@

$(LEONOS_HOST_TEST_BINS): $(O_HOST)/obj/tests/host/test_common.c.o $(LEONOS_HOST_COMMON_OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(O_HOST)/tests-sanitised/test_common: $(O_HOST)/obj/tests-sanitised/host/test_common.c.o \
	$(patsubst $(O_HOST)/obj/tools/host/common/%.c.o,$(O_HOST)/obj/tests-sanitised/tools/host/common/%.c.o,$(LEONOS_HOST_COMMON_OBJS))
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(LEONOS_SANITISE) -g -O1 $^ -o $@

LEONOS_SIG_host-cc-sanitised := argv=$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) -g -O1|path=$(leonos_host_tool_path)|identity=$(leonos_host_tool_identity)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,host-cc-sanitised)))

-include $(shell find $(O_HOST)/obj/tests $(O_HOST)/obj/tests-sanitised -name '*.o.d' 2>/dev/null)
