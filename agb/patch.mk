AGB_BINS :=	\
	build/render_sjis.bin \
	build/render_sjis_entry.bin \
	build/render_sjis_entry_menu.bin \
	build/clear_oam.bin \
	build/render_backlog_controls.bin \
	build/render_load_menu.bin

AGB_SECTIONS := \
	.text \
	.entry \
	.entry_menu \
	.clear_oam \
	.render_backlog_controls \
	.render_load_menu

AGB_OFFSETS := \
	7955180 \
	19876 \
	20368 \
	29184 \
	30540 \
	41360

AGB_BINS_SECTIONS := $(call pairup,$(AGB_BINS),$(AGB_SECTIONS))
AGB_BINS_OFFSETS := $(call pairup,$(AGB_BINS),$(AGB_OFFSETS))

# $(1): output .bin
# $(2): elf to read a section from
# $(3): section name
define BIN_TGT
$(1): $(2)
	@echo objcopy $$(notdir $$@)
	$$(VERBOSE) $$(ENV) $$(OBJCOPY) -O binary -j $(3) $$< $$@
endef

$(foreach agb_bin_section,$(AGB_BINS_SECTIONS), \
	$(eval bin = $(word 1,$(subst :, ,$(agb_bin_section)))) \
	$(eval section = $(word 2,$(subst :, ,$(agb_bin_section)))) \
	$(eval out = $(subst .bin,,$(bin))) \
	$(eval $(call BIN_TGT,$(bin),build/render_sjis,$(section))))
