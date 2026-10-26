CC         ?= gcc
NASM       ?= nasm
PKG_CONFIG ?= pkg-config

NATIVE     ?= 0
DEBUG      ?= 0
BUILD      := build

GLFW_CFLAGS := $(shell $(PKG_CONFIG) --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell $(PKG_CONFIG) --libs glfw3 2>/dev/null)

ifeq ($(DEBUG),1)
OPT_FLAGS := -O0 -g3 -fno-omit-frame-pointer
else
OPT_FLAGS := -O3 -DNDEBUG -fomit-frame-pointer
endif

ifeq ($(NATIVE),1)
MARCH_FLAGS := -march=native
else
MARCH_FLAGS := -march=x86-64-v2
endif

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wformat=2

CFLAGS     := -std=gnu11 $(OPT_FLAGS) $(MARCH_FLAGS) $(WARN_FLAGS) \
              -D_POSIX_C_SOURCE=200809L -fno-plt -pthread \
              -Iinclude -Ithird_party/glad/include $(GLFW_CFLAGS)
NASMFLAGS  := -f elf64
LDFLAGS    := -Wl,-O1,--as-needed -Wl,-z,relro,-z,now -pthread
LDLIBS     := $(GLFW_LIBS) -lm -ldl

ENGINE_BIN := $(BUILD)/engine/thinkcraft
TEST_MATH  := $(BUILD)/tests/test_math
TEST_CHUNK := $(BUILD)/tests/test_chunk_mesh
TEST_WORLD := $(BUILD)/tests/test_world
TEST_PLAYER := $(BUILD)/tests/test_player
TEST_TEXTURE := $(BUILD)/tests/test_texture

ENGINE_C_SRCS := \
	src/main.c \
	src/app.c \
	src/core/arena.c \
	src/core/log.c \
	src/core/platform.c \
	src/gfx/mesh.c \
	src/gfx/renderer.c \
	src/gfx/shader.c \
	src/gfx/texture.c \
	src/gfx/window.c \
	src/math/mat4.c \
	src/player/player.c \
	src/world/chunk.c \
	src/world/chunk_mesh.c \
	src/world/world.c \
	third_party/glad/src/gl.c

ENGINE_ASM_SRCS := src/asm/mat4_mul.asm src/asm/chunk_count_solid.asm

ENGINE_OBJS := \
	$(patsubst %.c,$(BUILD)/engine/obj/%.o,$(ENGINE_C_SRCS)) \
	$(patsubst %.asm,$(BUILD)/engine/obj/%.o,$(ENGINE_ASM_SRCS))

TEST_MATH_OBJS := \
	$(BUILD)/tests/obj/tests/test_math.o \
	$(BUILD)/tests/obj/src/math/mat4.o \
	$(BUILD)/tests/obj/src/asm/mat4_mul.o

TEST_CHUNK_OBJS := \
	$(BUILD)/tests/obj/tests/test_chunk_mesh.o \
	$(BUILD)/tests/obj/src/world/chunk.o \
	$(BUILD)/tests/obj/src/world/chunk_mesh.o \
	$(BUILD)/tests/obj/src/gfx/mesh.o \
	$(BUILD)/tests/obj/src/core/arena.o \
	$(BUILD)/tests/obj/src/core/log.o \
	$(BUILD)/tests/obj/src/core/platform.o \
	$(BUILD)/tests/obj/src/asm/chunk_count_solid.o

TEST_WORLD_OBJS := \
	$(BUILD)/tests/obj/tests/test_world.o \
	$(BUILD)/tests/obj/src/world/world.o \
	$(BUILD)/tests/obj/src/world/chunk.o \
	$(BUILD)/tests/obj/src/world/chunk_mesh.o \
	$(BUILD)/tests/obj/src/gfx/mesh.o \
	$(BUILD)/tests/obj/src/core/arena.o \
	$(BUILD)/tests/obj/src/core/log.o \
	$(BUILD)/tests/obj/src/core/platform.o \
	$(BUILD)/tests/obj/src/math/mat4.o \
	$(BUILD)/tests/obj/src/asm/mat4_mul.o \
	$(BUILD)/tests/obj/src/asm/chunk_count_solid.o

TEST_PLAYER_OBJS := \
	$(BUILD)/tests/obj/tests/test_player.o \
	$(BUILD)/tests/obj/src/player/player.o \
	$(BUILD)/tests/obj/src/world/world.o \
	$(BUILD)/tests/obj/src/world/chunk.o \
	$(BUILD)/tests/obj/src/world/chunk_mesh.o \
	$(BUILD)/tests/obj/src/gfx/mesh.o \
	$(BUILD)/tests/obj/src/core/arena.o \
	$(BUILD)/tests/obj/src/core/log.o \
	$(BUILD)/tests/obj/src/core/platform.o \
	$(BUILD)/tests/obj/src/math/mat4.o \
	$(BUILD)/tests/obj/src/asm/mat4_mul.o \
	$(BUILD)/tests/obj/src/asm/chunk_count_solid.o

TEST_TEXTURE_OBJS := \
	$(BUILD)/tests/obj/tests/test_texture.o \
	$(BUILD)/tests/obj/src/gfx/texture.o

all: engine

engine: check-deps $(ENGINE_BIN)

$(ENGINE_BIN): $(ENGINE_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/engine/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/engine/obj/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

$(TEST_MATH): $(TEST_MATH_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -lm -o $@

$(TEST_CHUNK): $(TEST_CHUNK_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -lm -o $@

$(TEST_WORLD): $(TEST_WORLD_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -lm -o $@

$(TEST_PLAYER): $(TEST_PLAYER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -lm -o $@

$(TEST_TEXTURE): $(TEST_TEXTURE_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -lm -o $@

$(BUILD)/tests/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/tests/obj/src/asm/%.o: src/asm/%.asm
	@mkdir -p $(dir $@)
	$(NASM) $(NASMFLAGS) $< -o $@

check-deps:
	@$(PKG_CONFIG) --exists glfw3 || { \
		echo "GLFW development files not found."; \
		echo "Install them with:"; \
		echo "  sudo apt update && sudo apt install -y build-essential nasm pkg-config libglfw3-dev libgl1-mesa-dev"; \
		exit 1; \
	}

test: test-math test-chunk-mesh test-world test-player test-texture

test-math: $(TEST_MATH)
	./$(TEST_MATH)

test-chunk-mesh: $(TEST_CHUNK)
	./$(TEST_CHUNK)

test-world: $(TEST_WORLD)
	./$(TEST_WORLD)

test-player: $(TEST_PLAYER)
	./$(TEST_PLAYER)

test-texture: $(TEST_TEXTURE)
	./$(TEST_TEXTURE)

test-all: test-math test-chunk-mesh test-world test-player test-texture test-transpiler

test-transpiler:
	$(MAKE) -C tools/transpiler test

transpiler:
	$(MAKE) -C tools/transpiler

run: engine
	./$(ENGINE_BIN)

benchmark: engine
	./$(ENGINE_BIN) --benchmark --seconds 10

clean:
	rm -rf $(BUILD)
	$(MAKE) -C tools/transpiler clean

-include $(ENGINE_OBJS:.o=.d) $(TEST_MATH_OBJS:.o=.d) $(TEST_CHUNK_OBJS:.o=.d) $(TEST_WORLD_OBJS:.o=.d) $(TEST_PLAYER_OBJS:.o=.d) $(TEST_TEXTURE_OBJS:.o=.d)

.PHONY: all engine check-deps test test-all test-math test-chunk-mesh test-world test-player test-texture test-transpiler transpiler run benchmark clean
