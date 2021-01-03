default: all

include ../rules.mk
include patch.mk

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

.PHONY: clean all
.SUFFIXES:

all: $(AGB_BINS) | build

-include $(DEP)

build:
	@mkdir -p build

build/%.o: %.c build
	@echo cc $<
	$(VERBOSE) $(ENV) $(CC) $(CFLAGS) -MMD -MT $@ -MF build/$*.d -o $@ -c $<

build/render_sjis: $(OBJ)
	@echo ld $(notdir $@)
	$(VERBOSE) $(ENV) $(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -rf build
