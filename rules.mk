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

ifeq ($V, 1)
	VERBOSE =
else
	VERBOSE = @
endif
