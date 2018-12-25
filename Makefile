ifeq ($V, 1)
	VERBOSE =
else
	VERBOSE = @
endif

include config.mk

SRC := \
	src/main.c \
	src/script.c \
	src/crc32.c \
	src/script_handlers.c \
	src/sjis_to_u8.c \
	src/strtab.c \
	src/branch.c

OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:%.o=%.d)

TARGET := build/shpn_tool

all: $(TARGET) | build

.PHONY: clean all
.SUFFIXES:

-include $(DEP)

build:
	@mkdir -p build

build/%.o: src/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	$(VERBOSE) $(ENV) $(CC) $(CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF build/$*.d -o $@ -c $<

$(TARGET).sym: $(OBJ) $(OBJ_PARSER)
	@echo ld $(notdir $@)
	$(VERBOSE) $(ENV) $(LD) $(LDFLAGS) $(LDLIBS) -o $@ $(OBJ)

$(TARGET): $(TARGET).sym
	@echo strip $(notdir $@)
	$(VERBOSE) $(ENV) $(STRIP) $(TARGET).sym -o $@

clean:
	@rm -rf build
