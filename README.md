# ThinkCraft

ThinkCraft is a lightweight, Minecraft-like engine written in C11 and x86-64 NASM. It uses OpenGL 3.3 core through GLFW and Glad, with a focus on predictable performance, a small dependency surface, and assembly hot paths.

The project is currently an engine foundation with a playable first-person demo, not a full Minecraft clone. The current milestone streams a 7x7 ring of 16x256x16 voxel chunks around the player, generates deterministic terrain, meshes each chunk with neighbor-aware greedy meshing, and renders it with a first-person controller and distance fog.

## Status

- Platform: Linux x86-64, X11 or XWayland
- Graphics: OpenGL 3.3 core, GLFW 3, Glad
- Language: C11 with x86-64 NASM
- Runtime: no JVM, no garbage collector, no engine scripting runtime
- Target hardware: Intel UHD 620 / i3-10110U-class laptops
- Target: stable 100 FPS at 1280x720

The current streaming-world benchmark runs at roughly 1,200 to 1,500 FPS on the development Intel UHD 620 machine at 1280x720 with vsync disabled. It renders a 7x7 ring around the player, remeshes boundary chunks when the player crosses a chunk edge, and uses distance fog to hide the streaming boundary.

## Current Milestone

- 1280x720 OpenGL 3.3 core window
- GLFW window creation, key-state input, mouse capture, and clean shutdown
- Glad-generated OpenGL loader
- Shader loading and compilation with error reporting
- Depth testing, back-face culling, and distance fog
- 16x256x16 chunk storage with inline bounds-checked block access
- Dynamic 7x7 chunk streaming ring around the player
- Deterministic demo terrain independent of streaming history
- Negative world and chunk coordinates through floor division
- Automatic load/unload of chunk rows and columns as the player moves
- Dirty-slot remeshing with neighbor updates across chunk borders
- Neighbor-aware greedy meshing across all three axes with internal-face culling
- Initial world reduced from 35,042 visible faces to 4,515 quads (7.76x)
- 49 per-chunk vertex-colored meshes uploaded to VBOs and EBOs
- First-person WASD controller with mouse look and Ctrl sprint
- Gravity, jumping, AABB voxel collision, one-block auto-step, and surface spawning
- Procedural stone, grass, dirt, and sand demo terrain
- Arena allocator for startup allocations
- 4x4 matrix math in C, including `look_at`
- `tc_mat4_mul` implemented in x86-64 NASM
- `tc_chunk_count_solid` implemented in x86-64 NASM
- Frame-time and FPS reporting
- Automated render benchmark and mesh benchmark output
- Chunk, world, player, and mesh tests covering boundary culling, streaming,
  generation determinism, negative coordinates, collision, jumping, and look limits

## Requirements

Linux x86-64 with:

- GCC or Clang
- GNU Make
- NASM
- pkg-config
- GLFW 3 development files
- OpenGL development files and a working OpenGL 3.3 driver
- `unzip` if you want to build the optional transpiler experiment

On Ubuntu or Debian:

```sh
sudo apt update && sudo apt install -y build-essential nasm pkg-config libglfw3-dev libgl1-mesa-dev unzip
```

The engine does not include GLFW source in this repository. It links against the system GLFW library. Glad is vendored under `third_party/glad`.

## Build

```sh
make
```

The binary is written to:

```text
build/engine/thinkcraft
```

Useful build variants:

```sh
make -j"$(nproc)"     # parallel build
make DEBUG=1          # -O0 with debug symbols
make NATIVE=1         # -march=native instead of the x86-64-v2 baseline
make clean
```

Release builds use `-march=x86-64-v2` by default so the same binary can run on a wider range of x86-64 machines. `NATIVE=1` is intended for local performance work, not distribution.

## Run

```sh
make run
```

or directly:

```sh
./build/engine/thinkcraft
```

Controls:

```text
Left click    capture the mouse
WASD          move
Mouse         look around
Space         jump
Ctrl          sprint
Escape        release the mouse; press again to close the window
```

The world streams around the player. At startup the engine prints the streaming-ring dimensions, solid block count, visible face count, greedy quad count, reduction factor, mesh build time, and draw-call count. When the player crosses a chunk boundary, the console reports how many chunks were remeshed and how long the update took.

Command-line options:

```text
--width N       window width in pixels
--height N      window height in pixels
--vsync         enable vertical sync
--no-vsync      disable vertical sync
--benchmark     exit after the benchmark duration
--seconds N     benchmark duration in seconds
--help          show help
```

The interactive default enables vsync. Benchmark mode disables vsync unless `--vsync` is explicitly passed.

## Benchmark

```sh
make benchmark
```

Equivalent to:

```sh
./build/engine/thinkcraft --benchmark --seconds 10
```

Example with a custom resolution:

```sh
./build/engine/thinkcraft --width 1920 --height 1080 --benchmark --seconds 10
```

The benchmark prints the elapsed time, total frames, average FPS, and average milliseconds per frame.

## Tests

```sh
make test
```

`make test` runs the portable math, chunk-mesh, world, and player test suites.

Individual test targets:

```sh
make test-math
make test-chunk-mesh
make test-world
make test-player
```

The math test compares the NASM matrix multiplication against a scalar C reference and checks perspective, view matrices, and negative-coordinate floor division. The chunk test covers chunk access, solid counting in NASM, greedy merge cases, index bounds, face reduction, and a 32-build meshing benchmark. The world test covers streaming updates, negative chunk coordinates, deterministic terrain reloads, world-space block access, generation, per-chunk rendering bounds, and neighbor-aware boundary culling. The player test covers gravity, ground contact, jumping, wall collision, surface spawning, pitch clamping, and yaw wrapping.

To run the math, chunk, world, player, and optional transpiler tests:

```sh
make test-all
```

`test-transpiler` builds and runs the transpiler proof of concept. It requires `unzip` and a local Minecraft client jar supplied through `MC_JAR`:

```sh
MC_JAR=/path/to/client.jar make test-transpiler
```

Build artifacts are kept under `build/` and are not committed.

## Project Layout

```text
include/thinkcraft/     Public engine headers
src/main.c              Entry point
src/app.c               Application loop and command-line handling
src/core/               Arena allocator, logging, platform helpers
src/gfx/                Window, shader, mesh, and renderer code
src/math/               Matrix math in C
src/player/             First-person movement and voxel collision
src/world/              Chunk storage, streaming world, demo terrain, and greedy meshing
src/asm/                NASM hot paths
shaders/                GLSL shaders
third_party/glad/       Vendored Glad 3.3 core loader
tools/transpiler/       Optional Java class-file transpiler experiment
tests/                  Unit tests
assets/cc0/             Committed CC0-only assets
assets/local/           Ignored local assets
LICENSES/               Third-party license texts
scripts/                Development helper scripts
```

## Design Principles

- Keep the engine small enough to understand and profile.
- Avoid per-frame heap allocation in the render loop.
- Prefer flat arrays, arena allocation, and predictable data layouts.
- Use assembly only where it is measurable and maintainable, starting with matrix multiplication.
- Keep the engine runtime free of Java and JVM dependencies.
- Prefer explicit ownership and deterministic cleanup over hidden global state.
- Make performance decisions measurable with a repeatable benchmark.
- Keep the dependency surface small and document every external component.
- Never ship copyrighted Minecraft assets or Mojang code.

## Performance Notes

The project targets frame times that leave headroom on integrated graphics rather than merely hitting a benchmark number. The current path uses flat `uint8_t` block arrays, an inline index, a three-axis greedy mask, neighbor-aware sampling across chunk borders, one indexed draw per chunk, and a direct-mapped 7x7 streaming ring. Important planned work includes:

- Texture atlases to replace vertex colors
- Frustum culling
- Chunk visibility scheduling
- Spreading or offloading boundary remeshing to avoid transition spikes
- Multithreaded mesh generation
- Avoiding per-frame allocation and unnecessary copying
- Native assembly paths only for verified hotspots

The current benchmark renders 49 chunks around the player. At startup the streaming ring has 167,983 solid blocks and 35,042 visible faces reduced to 4,515 quads; initial mesh building takes roughly 18 ms total on the development machine after warmup. Crossing a chunk boundary remeshes about 21 chunks, including new edge chunks and their affected neighbors, in roughly 20 to 30 ms. Rendering averages roughly 1,200 to 1,500 FPS at 1280x720.

## Optional Transpiler Experiment

`tools/transpiler/` is a separate research tool that reads selected Java class files, builds an intermediate representation, and emits NASM. It was used to prove that a focused bytecode-to-assembly path is possible.

The transpiler is not linked into the engine and is not required to build or run ThinkCraft. It also does not make the runtime depend on Java. Its generated output, class files, and jar links are build artifacts and are ignored.

The included test targets Minecraft's `CompactVectorArray` class for validation. Do not commit extracted game code or generated Minecraft-derived artifacts.

## Assets and Licensing

ThinkCraft is released under the MIT License. See `LICENSE`.

Asset rules:

- Only CC0 1.0 assets may be committed under `assets/cc0/`.
- `assets/local/` is ignored and is only for local experiments.
- Do not commit Mojang assets, extracted game files, purchased texture packs, or assets with unclear licensing.
- Do not commit generated code from proprietary class files.
- Record the source and license of any newly added committed asset.

Third-party components:

- Glad: MIT-licensed generator output, with Khronos specification terms included in `third_party/glad/LICENSE`.
- GLFW: system dependency under the zlib/libpng license.
- CC0 1.0 license text: `LICENSES/CC0-1.0.txt`.

See `THIRD_PARTY_NOTICES.md` for a summary.

## Roadmap

- [x] First OpenGL window and rotating textured cube
- [x] Shader loader, arena allocator, and NASM matrix multiply
- [x] FPS reporting and benchmark mode
- [x] Chunk storage and block registry
- [x] Chunk meshing
- [x] Greedy meshing
- [x] Multiple chunks and neighbor-aware meshing
- [x] Vertex-colored world rendering
- [x] Deterministic demo terrain across a chunk grid
- [ ] Texture atlas and textured blocks
- [ ] Dynamic chunk loading and unloading
- [ ] Player controller and collision
- [ ] Full world generation and biomes
- [ ] Save and load format
- [ ] Frustum culling and chunk scheduling
- [ ] Multithreaded mesh building
- [ ] Portable release packaging
- [ ] Windows build investigation

## Disclaimer

ThinkCraft is an independent engine project. It is not affiliated with, endorsed by, or sponsored by Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios. ThinkCraft does not include or distribute Minecraft assets.
