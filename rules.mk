define COMPILE_C
$(1)/%.o: $(2)/%.c
	@echo cc $$<
	@mkdir -p $$(dir $$@)
	$$(VERBOSE) $(ENV) $$(CC) $$(CFLAGS) $$(INC) $$(DEF) -MMD -MT $$@ -MF $(1)/$$*.d -o $$@ -c $$<
endef

define LINK_TARGET
$(1): $(2)
ifeq ($$(HAS_ICONV),0)
	@echo iconv.h has not been found\; string coding conversion will be unavailable. Set ICONV to \
specify iconv installation prefix.
endif
	@echo ld $$(notdir $$@)
	$$(VERBOSE) $$(ENV) $$(LD) $$(LDFLAGS) $$(LDLIBS) -o $$@ $2
endef

NUL :=
\t := $(NUL)	$(NUL)
define \n

$(NUL)
endef

# https://stackoverflow.com/a/34810883
pairup = $(if $1$2,$(firstword $1):$(firstword $2) $(call pairup,$(wordlist 2,$(words $1),$1),$(wordlist 2,$(words $2),$2)))

ifeq ($V, 1)
	VERBOSE =
else
	VERBOSE = @
endif

BUILD_TAG := $(shell git describe --tags 2> /dev/null || echo `git symbolic-ref HEAD 2> /dev/null |\
	cut -b 12-`-`git log --pretty=format:%h -1`)

define MAKE_IPS
build/$(1).ips: build/$(1).rom
	@echo ips $(1)
	$$(VERBOSE) $(ENV) $$(FLIPS) -c --ips $(SHPN_ROM) build/$(1).rom build/$(1).ips
endef

define MAKE_BPS
build/$(1).bps: build/$(1).rom
	@echo bps $(1)
	$$(VERBOSE) $(ENV) $$(FLIPS) -c --bps $(SHPN_ROM) build/$(1).rom build/$(1).bps
endef

define MAKE_ROM
build/$(1).rom: build/Cybil.$(1).rom $(AGB_BINS=%:build/%)
	@echo make_rom $(1)
	$$(VERBOSE) $(ENV) cp build/Cybil.$(1).rom build/$(1).rom
	$(foreach agb_bin_offs,$(AGB_BINS_OFFSETS),\
	$(eval bin = $(word 1,$(subst :, ,$(agb_bin_offs)))) \
	$(eval offs = $(word 2,$(subst :, ,$(agb_bin_offs)))) \
	$(VERBOSE) $(ENV) dd if=agb/$(bin) of=build/$(1).rom conv=notrunc bs=1 seek=$(offs)
	)
endef
