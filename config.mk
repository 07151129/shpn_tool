CC ?= cc
LD := $(CC)
STRIP ?= strip

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

ifeq ($(DEBUG), 1)
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
