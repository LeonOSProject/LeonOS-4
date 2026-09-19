# Test targets.
#
# The core harness is POSIX shell and C: the acceptance rule for this migration
# may not depend on the interpreter it is auditing (plan section 2).

LEONOS_TEST_TMP := $(if $(TMPDIR),$(TMPDIR),/tmp)/leonos-tests-$(shell id -u)

# C unit tests for the shared host primitives and the JSON reader.
LEONOS_HOST_TEST_BINS := $(O_HOST)/tests/test_common $(O_HOST)/tests/test_json
LEONOS_HOST_TEST_SANITISED := $(O_HOST)/tests-sanitised/test_common \
	$(O_HOST)/tests-sanitised/test_json

# Shell contract tests. test-bootstrap.sh is the public entry-point surface.
LEONOS_BUILD_TESTS := $(sort $(wildcard $(LEONOS_SRC)/tests/build/test-*.sh))
# Suites that are too long to run on every change but must not be forgotten:
# repeated -j1/-j8 comparisons and, later, guest boots. Plan section 13 forbids
# presenting an unrun long test as a skipped pass, so they are a separate goal.
LEONOS_LONG_TESTS := $(sort $(wildcard $(LEONOS_SRC)/tests/long/test-*.sh))

# The plain and sanitised suites are separate goals so `test-tools` keeps a
# single recipe: the plan requires ASan/UBSan at the tool boundary, not merely a
# clean compile.
.PHONY: leonos-test-tools-plain leonos-test-tools-sanitised

test-tools: leonos-test-tools-plain leonos-test-tools-sanitised

leonos-test-tools-plain: $(LEONOS_HOST_TEST_BINS)
	@set -eu; for test_binary in $(LEONOS_HOST_TEST_BINS); do \
	    $(call LEONOS_LOG_SHELL,RUN,$$test_binary); \
	    $$test_binary; \
	done

leonos-test-tools-sanitised: $(LEONOS_HOST_TEST_SANITISED)
	@set -eu; for test_binary in $(LEONOS_HOST_TEST_SANITISED); do \
	    $(call LEONOS_LOG_SHELL,RUN,$$test_binary); \
	    ASAN_OPTIONS=detect_leaks=1 \
	    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 $$test_binary; \
	done

# Contract tests get the built tool and the lock file by name so they never
# reach for a stale copy left in the output tree.
#
# A suite can report ok lines while still exiting 0 on a failure it counted
# itself, and it can exit non-zero after printing nothing. Both are checked: the
# status *and* any `FAIL - ` line in the captured report.
#
# $(call LEONOS_RUN_CONTRACT_TESTS,tests...)
define LEONOS_RUN_CONTRACT_TESTS
	set -eu; for contract_test in $(1); do \
	    $(call LEONOS_LOG_SHELL,RUN,$$contract_test); \
	    report=$$(mktemp); \
	    if LEONOS_DEPS='$(LEONOS_DEPS_TOOL)' \
	       LEONOS_LOCK='$(LEONOS_SRC)/configs/dependencies.lock.json' \
	       LEONOS_EMIT='$(LEONOS_EMIT)' \
	       sh $$contract_test >$$report 2>&1; then status=0; else status=1; fi; \
	    cat $$report; \
	    if grep -q 'FAIL - ' $$report; then status=1; fi; \
	    rm -f $$report; \
	    if [ $$status -ne 0 ]; then \
	        printf 'not ok - %s reported a failure\n' $$contract_test; exit 1; \
	    fi; \
	done
endef

test-build: test-tools $(LEONOS_HOST_TOOLS)
	@$(call LEONOS_RUN_CONTRACT_TESTS,$(LEONOS_BUILD_TESTS))

test-long: $(LEONOS_HOST_TOOLS)
	@$(call LEONOS_RUN_CONTRACT_TESTS,$(LEONOS_LONG_TESTS))

test: test-tools test-build

$(O_HOST)/obj/tests/host/%.c.o: $(LEONOS_SRC)/tests/host/%.c $(O_META)/host-cc.sig
	$(Q)mkdir -p $(dir $@)
	$(call LEONOS_LOG,HOSTCC,$<)
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) -I$(LEONOS_SRC)/tests/host \
	    $(LEONOS_HOST_INCLUDES) $(HOST_CFLAGS) -MMD -MF $@.d -c $< -o $@

$(O_HOST)/obj/tests-sanitised/host/%.c.o: $(LEONOS_SRC)/tests/host/%.c $(O_META)/host-cc-sanitised.sig
	$(Q)mkdir -p $(dir $@)
	$(call LEONOS_LOG,HOSTCC,$<)
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) \
	    -I$(LEONOS_SRC)/tests/host $(LEONOS_HOST_INCLUDES) -g -O1 \
	    -MMD -MF $@.d -c $< -o $@

$(O_HOST)/obj/tests-sanitised/tools/host/%.c.o: $(LEONOS_SRC)/tools/host/%.c \
	$(O_META)/host-cc-sanitised.sig
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) -g -O1 \
	    $(LEONOS_HOST_INCLUDES) -MMD -MF $@.d -c $< -o $@

$(O_HOST)/tests/test_common: $(O_HOST)/obj/tests/host/test_common.c.o $(LEONOS_HOST_COMMON_OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

$(O_HOST)/tests/test_json: $(O_HOST)/obj/tests/host/test_json.c.o $(LEONOS_JSON_OBJ) \
	$(LEONOS_HOST_COMMON_OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@

LEONOS_HOST_SANITISED_OBJS := $(patsubst $(O_HOST)/obj/tools/host/%.c.o, \
	$(O_HOST)/obj/tests-sanitised/tools/host/%.c.o,$(LEONOS_HOST_COMMON_OBJS))

$(O_HOST)/tests-sanitised/test_common: $(O_HOST)/obj/tests-sanitised/host/test_common.c.o \
	$(LEONOS_HOST_SANITISED_OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(LEONOS_SANITISE) -g -O1 $^ -o $@

$(O_HOST)/tests-sanitised/test_json: $(O_HOST)/obj/tests-sanitised/host/test_json.c.o \
	$(O_HOST)/obj/tests-sanitised/tools/host/manifest/json.c.o $(LEONOS_HOST_SANITISED_OBJS)
	$(Q)mkdir -p $(dir $@)
	$(Q)$(HOSTCC) $(LEONOS_SANITISE) -g -O1 $^ -o $@

LEONOS_SIG_host-cc-sanitised := argv=$(HOSTCC) $(LEONOS_STRICT_WARNINGS) $(LEONOS_SANITISE) -g -O1|path=$(leonos_host_tool_path)|identity=$(leonos_host_tool_identity)
$(if $(LEONOS_PASSIVE),,$(eval $(call LEONOS_SIGNATURE_RULE,host-cc-sanitised)))

-include $(shell find $(O_HOST)/obj/tests $(O_HOST)/obj/tests-sanitised -name '*.o.d' 2>/dev/null)
.PHONY: test test-tools test-build test-long
