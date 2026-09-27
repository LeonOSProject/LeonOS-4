# The CJK font generation (cjk_font.h, its unifont dependency closure and the
# framebuffer.c.o prerequisite) belongs to the kernel checkout since phase 3:
# the only consumer was drivers/bootstrap/framebuffer.c, which is built there.
LEONOS_FONT_TOOL := $(LEONOS_HOST_BIN)/leonos-font
LEONOS_GRUB_FONT_TOOL := $(LEONOS_HOST_BIN)/leonos-grub-font
$(LEONOS_FONT_TOOL): $(O_HOST)/obj/tools/host/assets/leonos-font.c.o $(LEONOS_HOST_COMMON_OBJS)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
$(LEONOS_GRUB_FONT_TOOL): $(O_HOST)/obj/tools/host/assets/leonos-grub-font.c.o $(LEONOS_HOST_COMMON_OBJS)
	$(Q)$(HOSTCC) $(HOST_CFLAGS) $(HOST_LDFLAGS) $^ -o $@
UI_METRO_FONT := $(O_GENERATED)/fonts/leonos-metro.ttf
UI_WIN95_FONT := $(O_GENERATED)/fonts/leonos-win95.ttf
GRUB_BDF := $(O_GENERATED)/grub/leonos-pixel.bdf
GRUB_FONT := $(O_GENERATED)/grub/leonos-unicode.pf2
$(UI_METRO_FONT) $(UI_WIN95_FONT) &: $(LEONOS_FONT_TOOL) $(LEONOS_SRC)/system/fonts/Deng.ttf $(LEONOS_SRC)/system/fonts/system.psf
	$(Q)mkdir -p $(@D)
	$(Q)$(LEONOS_FONT_TOOL) $(LEONOS_SRC)/system/fonts/Deng.ttf $(LEONOS_SRC)/system/fonts/system.psf $(UI_METRO_FONT) $(UI_WIN95_FONT)
$(GRUB_BDF): $(LEONOS_GRUB_FONT_TOOL) $(LEONOS_SRC)/system/fonts/system.psf
	$(Q)mkdir -p $(@D)
	$(Q)$(LEONOS_GRUB_FONT_TOOL) $(LEONOS_SRC)/system/fonts/system.psf $@
$(GRUB_FONT): $(GRUB_BDF)
	$(Q)grub-mkfont -s 16 -o $@.tmp $<
	$(Q)mv $@.tmp $@
COMPONENT_METADATA := $(O_CONFIG)/components.tsv
$(COMPONENT_METADATA): $(LEONOS_COMPONENT_TOOL) $(LEONOS_COMPONENT_MK) $(LEONOS_CONFIG_FILE)
	$(Q)$(LEONOS_COMPONENT_TOOL) --input $(LEONOS_SRC)/configs/components.toml --config $(LEONOS_CONFIG_FILE) --output $(LEONOS_COMPONENT_MK) --metadata $@
.PHONY: resources
resources: $(UI_METRO_FONT) $(UI_WIN95_FONT) $(GRUB_FONT) $(COMPONENT_METADATA)
tools: $(LEONOS_FONT_TOOL) $(LEONOS_GRUB_FONT_TOOL) $(LEONOS_COMPONENT_TOOL) $(LEONOS_GEARS_TOOL)
