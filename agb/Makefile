include ../rules.mk

ARMPREFIX := arm-none-eabi

CC := cc -target $(ARMPREFIX)
LD := $(ARMPREFIX)-ld
OBJCOPY := $(ARMPREFIX)-objcopy

LDSCRIPT := agb.ld

CFLAGS := \
	-Wall \
	-Wextra \
	-pedantic \
	-Oz \
	-std=c11 \
	-mthumb \
	-mcpu=arm7tdmi-s \
	-nostdlib \
	-static \
	-ffreestanding \
	-fno-strict-aliasing

LDFLAGS := \
	-static \
	-gc-sections \
	-nostartfiles \
	-T$(LDSCRIPT) \
	-N

.PHONY: clean all

all: render_sjis.bin render_sjis_entry.bin

render_sjis.o: render_sjis.c
	@echo cc $<
	$(VERBOSE) $(ENV) $(CC) $(CFLAGS) -o $@ -c $<

render_sjis: render_sjis.o
	@echo ld $@
	$(VERBOSE) $(ENV) $(LD) $(LDFLAGS) -o $@ $<

render_sjis.bin: render_sjis
	@echo objcopy $@
	$(VERBOSE) $(ENV) $(OBJCOPY) -O binary -j .text $< $@

render_sjis_entry.bin: render_sjis
	@echo objcopy $@
	$(VERBOSE) $(ENV) $(OBJCOPY) -O binary -j .entry $< $@

clean:
	rm -f render_sjis.bin render_sjis_entry.bin render_sjis.o render_sjis
