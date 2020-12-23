include ../rules.mk

ARMPREFIX := arm-none-eabi

CC := cc -target $(ARMPREFIX)
LD := $(ARMPREFIX)-ld
OBJCOPY := $(ARMPREFIX)-objcopy

LDSCRIPT := agb.ld

SRC := \
	render_sjis.c \
	glyph_margins.c \
	static_strings.c

OBJ := $(SRC:%.c=build/%.o)
DEP := $(OBJ:%.o=%.d)

CFLAGS := \
	-Wall \
	-Wextra \
	-pedantic \
	-Os \
	-std=c11 \
	-mthumb \
	-mcpu=arm7tdmi-s \
	-nostdlib \
	-static \
	-ffreestanding \
	-fno-strict-aliasing \
	-DFREESTANDING

LDFLAGS := \
	-static \
	-gc-sections \
	-nostartfiles \
	-T$(LDSCRIPT) \
	-N

BINS = build/render_sjis.bin build/render_sjis_entry.bin build/render_sjis_entry_menu.bin \
	build/clear_oam.bin build/render_backlog_controls.bin build/render_load_menu.bin

.PHONY: clean all
.SUFFIXES:

all: $(BINS) | build

-include $(DEP)

build:
	@mkdir -p build

build/%.o: %.c build
	@echo cc $<
	$(VERBOSE) $(ENV) $(CC) $(CFLAGS) -MMD -MT $@ -MF build/$*.d -o $@ -c $<

build/render_sjis: $(OBJ)
	@echo ld $(notdir $@)
	$(VERBOSE) $(ENV) $(LD) $(LDFLAGS) -o $@ $^

define BIN_TGT
$(1): $(2)
	@echo objcopy $$(notdir $$@)
	$$(VERBOSE) $$(ENV) $$(OBJCOPY) -O binary -j $(3) $$< $$@
endef

$(eval $(call BIN_TGT,build/render_sjis.bin,build/render_sjis,.text))
$(eval $(call BIN_TGT,build/render_sjis_entry.bin,build/render_sjis,.entry))
$(eval $(call BIN_TGT,build/render_sjis_entry_menu.bin,build/render_sjis,.entry_menu))
$(eval $(call BIN_TGT,build/clear_oam.bin,build/render_sjis,.clear_oam))
$(eval $(call BIN_TGT,build/render_backlog_controls.bin,build/render_sjis,.render_backlog_controls))
$(eval $(call BIN_TGT,build/render_load_menu.bin,build/render_sjis,.render_load_menu))

clean:
	rm -rf build
