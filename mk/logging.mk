# Central build-log formatting for GNU Make recipes.
#
# LEONOS_LOG is for a standalone recipe line and follows V=1 through Q.
# LEONOS_LOG_SHELL is for a command embedded in a shell loop or compound
# recipe, where Make's leading @ must not become part of the shell command.

LEONOS_LOG_FORMAT := '  %-8s %s\n'
LEONOS_LOG_COMMAND = printf $(LEONOS_LOG_FORMAT)

# $(call LEONOS_LOG,label,message)
define LEONOS_LOG
$(Q)$(LEONOS_LOG_COMMAND) "$(1)" "$(2)"
endef

# $(call LEONOS_LOG_SHELL,label,message)
define LEONOS_LOG_SHELL
$(LEONOS_LOG_COMMAND) "$(1)" "$(2)"
endef
