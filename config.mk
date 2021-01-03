CC ?= cc
LD := $(CC)
STRIP ?= strip
FLIPS ?= flips

YACC ?= bison
LEX ?= flex

SHPN_ROM ?= ../0019\ -\ Play\ Novel\ -\ Silent\ Hill\ \(J\).gba

CFLAGS_OPT := \
    -Os \
    -DNDEBUG

CFLAGS := \
    -std=c11 \
    -Wall \
    -Wextra \
    -pedantic \
    -Isrc \
    -fno-strict-aliasing \
    -I.

LDFLAGS :=

LDLIBS :=

CFLAGS_DEBUG := \
    -g \
    -O0 \
    -DDEBUG \
    -UNDEBUG

YACC_FLAGS := \
    -d \
    -Wno-yacc

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
	CFLAGS += -I$(ICONV)/include/ -DHAS_ICONV
	LDFLAGS += -L$(ICONV)/lib -liconv
endif
