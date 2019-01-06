CC ?= cc
LD := $(CC)
STRIP ?= strip

YACC ?= bison
LEX ?= flex

CFLAGS_OPT := \
    -Os \
    -DNDEBUG

CFLAGS := \
    -std=c11 \
    -Wall \
    -Wextra \
    -pedantic \
    -Isrc \
    -fno-strict-aliasing

LDFLAGS :=

LDLIBS :=

CFLAGS_DEBUG := \
    -g \
    -O0 \
    -DDEBUG \
    -UNDEBUG

YACC_FLAGS := \
    -y \
    -d

LEX_FLAGS :=

ifeq ($(DEBUG),1)
    CFLAGS += $(CFLAGS_DEBUG)
else
    CFLAGS += $(CFLAGS_OPT)
endif

ifneq ($(SANITIZE),)
	CFLAGS += -fsanitize=$(SANITIZE)
	LDFLAGS += -fsanitize=$(SANITIZE)
endif

HAS_ICONV := 0
ICONV ?= /usr

ifneq ($(wildcard $(ICONV)/include/iconv.h),)
	HAS_ICONV = 1
	CFLAGS += -I$(ICONV)/include/iconv.h -DHAS_ICONV
	LDFLAGS += -L$(ICONV)/lib -liconv
endif

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
