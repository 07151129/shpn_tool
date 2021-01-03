SCRIPTS := EN RU

VMA_HARRY := 0x8900000
SZ_HARRY := 1048576

VMA_CYBIL := 0x8a00000
SZ_CYBIL := 524288

STRTAB_SCRIPT_VMA := 0x8800000
STRTAB_SCRIPT_SZ := 675840

STRTAB_MENU_VMA := 0x88a5000
STRTAB_MENU_SZ := 372736

# shpn_tool doesn't support embedding multiple scripts in a single invocation, so embed them in
# two steps

# $(1): Language name
define EMBED_HARRY
build/Harry.$(1).rom: scripts/$(1)/Harry scripts/$(1)/strtab_menu scripts/$(1)/strtab_script build/shpn_tool
	$$(VERBOSE) $(ENV) \
	./build/shpn_tool $(SHPN_ROM) \
	script Harry $(VMA_HARRY) $(STRTAB_SCRIPT_VMA) $(STRTAB_MENU_VMA) \
	embed scripts/$(1)/Harry 0 $(SZ_HARRY) \
	scripts/$(1)/strtab_script scripts/$(1)/strtab_menu \
	$(STRTAB_SCRIPT_SZ) $(STRTAB_MENU_SZ) \
	build/Harry.$(1).rom
endef

define EMBED_CYBIL
build/Cybil.$(1).rom: scripts/$(1)/Cybil scripts/$(1)/strtab_menu scripts/$(1)/strtab_script build/Harry.$(1).rom
	$$(VERBOSE) $(ENV) \
	./build/shpn_tool build/Harry.$(1).rom \
	script Cybil $(VMA_CYBIL) $(STRTAB_SCRIPT_VMA) $(STRTAB_MENU_VMA) \
	embed scripts/$(1)/Cybil 1 $(SZ_CYBIL) \
	scripts/$(1)/strtab_script scripts/$(1)/strtab_menu \
	$(STRTAB_SCRIPT_SZ) $(STRTAB_MENU_SZ) \
	build/Cybil.$(1).rom
endef

$(foreach script,$(SCRIPTS), \
	$(eval $(call EMBED_HARRY,$(script))) \
	$(eval $(call EMBED_CYBIL,$(script))))
