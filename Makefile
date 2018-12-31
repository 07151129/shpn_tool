ifeq ($V, 1)
	VERBOSE =
else
	VERBOSE = @
endif

include config.mk

# File containing main must be the first in the list
SRC := \
	src/main.c \
	src/script_disass.c \
	src/crc32.c \
	src/script_handlers.c \
	src/strtab.c \
	src/branch.c

SRC_TEST := \
	test/make_strtab.c

OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:%.o=%.d)

OBJ_TEST := $(SRC_TEST:test/%.c=build/test/%.o)
OBJ_TEST += $(wordlist 2,$(words $(OBJ)),$(OBJ))
DEP_TEST := $(OBJ_TEST:%.o=%.d)

TARGET := build/shpn_tool
TARGETS_TEST := build/test/make_strtab.sym

all: $(TARGET) | build

.PHONY: clean all test
.SUFFIXES:

-include $(DEP) $(DEP_TEST)

build:
	@mkdir -p build

$(eval $(call COMPILE_C,build,src))
$(eval $(call COMPILE_C,build/test,test))

$(eval $(call LINK_TARGET,$(TARGET).sym,$(OBJ)))
$(eval $(call LINK_TARGET,$(TARGETS_TEST),$(OBJ_TEST)))

$(TARGET): $(TARGET).sym
	@echo strip $(notdir $@)
	$(VERBOSE) $(ENV) $(STRIP) $(TARGET).sym -o $@

clean:
	@rm -rf build

testdir:
	@mkdir -p build/test

test: $(OBJ) $(OBJ_TEST) $(TARGETS_TEST) | testdir
	@$(foreach tgt,$(TARGETS_TEST),@echo $(tgt);$(tgt))
