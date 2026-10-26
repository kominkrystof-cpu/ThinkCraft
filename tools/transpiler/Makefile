CC        := gcc
NASM      ?= nasm

MC_JAR    ?=
CLASS     ?= com/mojang/blaze3d/vertex/CompactVectorArray
PREFIX    ?= cva

BUILD     := ../../build/tools/transpiler
JARLINK   := $(BUILD)/mc.jar
CLASSF    := $(BUILD)/classes/$(CLASS).class
GEN       := $(BUILD)/generated/compact_vector_array.asm
OBJ       := $(BUILD)/obj/compact_vector_array.o
TRANS     := $(BUILD)/transpiler
TEST      := $(BUILD)/test_compact

CFLAGS    := -std=gnu11 -O2 -g -Wall -Wextra -Iinclude -Iruntime
NASMFLAGS := -f elf64 -g -F dwarf
LDFLAGS   := -no-pie

TRANS_SRCS   := $(wildcard src/*.c)
RUNTIME_SRCS := runtime/arena.c runtime/arrays.c

all: test

$(JARLINK):
	@test -n "$(MC_JAR)" || { \
		echo "MC_JAR is not set."; \
		echo "Set it to a local Minecraft client jar, for example:"; \
		echo "  MC_JAR=/path/to/client.jar make test"; \
		exit 1; \
	}
	mkdir -p $(BUILD)
	ln -sf "$(MC_JAR)" $@

$(TRANS): $(TRANS_SRCS) $(wildcard include/*.h) $(wildcard runtime/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(TRANS_SRCS) -o $@

$(CLASSF): $(JARLINK)
	mkdir -p $(dir $@)
	unzip -p $< "$(CLASS).class" > $@

$(GEN): $(TRANS) $(CLASSF)
	mkdir -p $(dir $@)
	$(TRANS) --class $(CLASSF) --out $@ --prefix $(PREFIX) --verbose

$(OBJ): $(GEN)
	mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

$(TEST): $(OBJ) tests/test_compact_vector_array.c $(RUNTIME_SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

test: $(TEST)
	./$(TEST)

asm: $(GEN)

clean:
	rm -rf $(BUILD)

.PHONY: all asm test clean
