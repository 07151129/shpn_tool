include config.mk
include rules.mk

# File containing main must be the first in the list
SRC := \
	src/main.c \
	src/script_disass.c \
	src/crc32.c \
	src/script_handlers.c \
	src/strtab.c \
	src/branch.c \
	src/script_as.c \
	src/script_parse_ctx.c \
	src/embed.c \
	src/search.c

SRC_TEST := \
	test/make_strtab.c \
	test/script_as.c \
	test/mk_strtab_str.c \
	test/embed_strtab.c

SRC_LEX := src/script_lex.yy.c
SRC_YACC := src/script_gram.tab.c

SRC_PARSER := $(SRC_LEX) $(SRC_YACC)

SRC += $(OBJ_LEX)
SRC += $(OBJ_YACC)

OBJ := $(SRC:src/%.c=build/%.o)
OBJ += $(SRC_PARSER:src/%.c=build/%.o)
DEP := $(OBJ:%.o=%.d)

# all but main.o
OBJ_TEST := $(wordlist 2,$(words $(OBJ)),$(OBJ))
DEP_TEST := $(OBJ_TEST:%.o=%.d)

TARGET := build/shpn_tool
TARGETS_TEST := $(SRC_TEST:test/%.c=build/test/%.sym)

all: $(TARGET) | build

.PHONY: clean all test help distclean yyclean
.SUFFIXES:

-include $(DEP) $(DEP_TEST)

build:
	@mkdir -p build

src/%.tab.c src/%.tab.h: src/%.y
	@echo yacc $(notdir $<)
	$(VERBOSE) $(ENV) $(YACC) $(YACC_FLAGS) -o $(@:%.h=%.c) $<

src/%.yy.c src/%.yy.h: src/%.l $(SRC_YACC)
	@echo lex $(notdir $<)
	$(VERBOSE) $(ENV) $(LEX) $(LEX_FLAGS) -o $(@:%.h=%.c) $<

$(eval $(call COMPILE_C,build,src))
$(eval $(call COMPILE_C,build/test,test))

build/script_parse_ctx.o: $(SRC_PARSER)

$(eval $(call LINK_TARGET,$(TARGET).sym,$(OBJ)))

# For each test target, link the objects from src/ (but main.o) and only the test .o we need
$(foreach test,$(TARGETS_TEST),$(eval $(call LINK_TARGET,$(test),$(OBJ_TEST) $(test:%.sym=%.o))))

$(TARGET): $(TARGET).sym
	@echo strip $(notdir $@)
	$(VERBOSE) $(ENV) $(STRIP) $(TARGET).sym -o $@

clean:
	@rm -rf build

yyclean:
	@rm -f $(SRC_PARSER) $(SRC_PARSER:src/%.c=src/%.h)

distclean: clean yyclean

testdir:
	@mkdir -p build/test

test: $(TARGETS_TEST) | testdir
	-$(foreach tgt,$(TARGETS_TEST),$(tgt)$(\n))

help:
	$(info Supported targets:)
	$(info all$(\t)$(\t)$(\t)compile everything but tests)
	$(info $(TARGET)$(\t)$(\t)compile $(TARGET))
	$(info $(TARGET).sym$(\t)compile symbolised $(TARGET))
	$(info test$(\t)$(\t)$(\t)run unit tests)
	$(info clean$(\t)$(\t)$(\t)remove build artefacts)
	$(info yyclean$(\t)$(\t)$(\t)remove $(SRC_PARSER))
	$(info distclean$(\t)$(\t)same as clean and yyclean)
	$(info help$(\t)$(\t)$(\t)show this message)
	$(info Supported environment variables:)
	$(info DEBUG$(\t)$(\t)$(\t)compile code with debug info, without optimisations)
	$(info ICONV$(\t)$(\t)$(\t)iconv installation prefix)
	$(info SANITIZE$(\t)$(\t)build with specified sanitizer (e.g. address))
	$(info VERBOSE$(\t)$(\t)$(\t)verbose build command logging)
	@:
