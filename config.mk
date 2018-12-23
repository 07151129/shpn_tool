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
