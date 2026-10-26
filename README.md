# ThinkCraft

ThinkCraft is a lightweight, Minecraft-like engine written in C11 and x86-64 NASM. It uses OpenGL 3.3 core through GLFW and Glad, with a focus on predictable performance, a small dependency surface, and assembly hot paths.

The project is currently an engine foundation with a playable first-person demo, not a full Minecraft clone. The current milestone streams a 9x9 ring of 16x256x16 voxel chunks around the player, generates seeded procedural terrain with caves, meshes each chunk with neighbor-aware greedy meshing, and renders it with a procedural pixel-art texture atlas, a first-person controller, block breaking and placing, an unbreakable bedrock floor, distance fog, a main/pause menu, and persistent world saves.

## Status

- Platform: Linux x86-64, X11 or XWayland
- Graphics: OpenGL 3.3 core, GLFW 3, Glad
- Language: C11 with x86-64 NASM
- Runtime: no JVM, no garbage collector, no engine scripting runtime
- Target hardware: Intel UHD 620 / i3-10110U-class laptops
- Target: stable 100 FPS at 1280x720

The current streaming-world benchmark runs at roughly 1,100 FPS on the development Intel UHD 620 machine at 1280x720 with vsync disabled. It renders a 9x9 ring around the player, frustum-culls to roughly 33 visible chunks per frame, draws visible chunks front-to-back, spreads boundary remeshing across frames when the player crosses a chunk edge, and uses distance fog to hide the streaming boundary.

## Milestone: Persistent Worlds and Menus

This milestone makes world state durable across sessions. ThinkCraft now boots into a main menu, supports New World and Continue, exposes a pause menu, renders UI text without external assets, and stores seeds, player pose, hotbar selection, and block edits in a versioned binary save.

- 1280x720 OpenGL 3.3 core window
- GLFW window creation, key-state input, mouse capture, and clean shutdown
- Glad-generated OpenGL loader
- Shader loading and compilation with error reporting
- Depth testing, back-face culling, and softened distance fog (48 to 80 blocks)
- 16x256x16 chunk storage with inline bounds-checked block access
- Dynamic 9x9 chunk streaming ring around the player
- Seeded 2D value-noise terrain independent of streaming history
- 3D noise caves carved beneath the surface with no surface breaches
- Negative world and chunk coordinates through floor division
- Automatic load/unload of chunk rows and columns as the player moves
- Budgeted dirty-slot remeshing with priority edits and deferred neighbor updates
- Neighbor-aware greedy meshing across all three axes with internal-face culling
- Initial world reduced from 94,178 visible faces to 17,199 quads (5.48x)
- 81 per-chunk textured meshes uploaded to VBOs and EBOs
- Frustum culling and front-to-back chunk drawing (roughly 33 of 81 chunks visible at spawn)
- Procedural 64x64 RGBA texture atlas uploaded as a 16-layer 2D texture array
- Textured grass, dirt, stone, sand, and bedrock with per-face shading
- Per-vertex distance fog and optional internal render scaling
- Repeating tile-local UVs on greedy-merged quads
- First-person WASD controller with mouse look and Ctrl sprint
- DDA voxel picking with a six-block reach
- Left/right click block breaking and placing with priority remeshing
- Four-slot hotbar with 1-4 selection, crosshair, and placement collision checks
- Gravity, jumping, AABB voxel collision, one-block auto-step, and surface spawning
- Unbreakable bedrock floor at y=0 and a hard void-floor collision clamp
- Procedural stone, grass, dirt, and sand terrain with seeded hills, sand patches, stone peaks, and caves
- Main menu with New World, Continue, and Quit
- Pause menu with Resume, Save, Load, and Quit to Menu
- Persistent binary saves containing the seed, player pose, hotbar selection, and block edits
- Built-in 5x7 uppercase bitmap text renderer for menu UI and status messages
- Arena allocator for startup allocations
- 4x4 matrix math in C, including `look_at`
- `tc_mat4_mul` implemented in x86-64 NASM
- `tc_chunk_count_solid` implemented in x86-64 NASM
- Frame-time and FPS reporting
- Automated render benchmark and mesh benchmark output
- Chunk, world, save, player, texture, raycast, and mesh tests covering
  boundary culling, streaming, generation determinism, seed stability, cave
  generation, bedrock protection, void-floor collision, negative
  coordinates, collision, jumping, look limits, atlas tile data, voxel
  picking, placement overlap, save round trips, and edit persistence across
  chunk reloads

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
Left click    capture the mouse; while captured, break the targeted block
Right click   place the selected block
WASD          move; navigate menus when paused
Mouse         look around
Space         jump; activate menu item
Ctrl          sprint
1-4           select grass, dirt, stone, or sand
Escape        release the mouse; press again to pause; from the main menu, quit
F5            quick save while playing or paused
F9            quick load while playing or paused
Up/Down       navigate menus
Enter/Space   activate menu item
```

The game starts at the main menu. New World regenerates the world from the configured seed, Continue loads the save at the configured path, and Quit exits. The pause menu can save or load without leaving the game. Saves default to `saves/world.tc`.

The world streams around the player. At startup the engine prints the seed, streaming-ring dimensions, solid block count, visible face count, greedy quad count, reduction factor, mesh build time, and draw-call count. When the player crosses a chunk boundary, terrain generation is queued and remeshing is spread across frames under a small time budget. Block edits rebuild the touched chunk first and defer neighbor updates by a frame; edits are also appended to a persistent edit log so they survive chunk unload/reload and saving.

Command-line options:

```text
--width N       window width in pixels
--height N      window height in pixels
--vsync         enable vertical sync
--no-vsync      disable vertical sync
--benchmark     exit after the benchmark duration
--seconds N     benchmark duration in seconds
--seed N        terrain seed (default 0x5eed1234)
--save PATH     save file path (default saves/world.tc)
--render-scale N  internal render scale 0.5-1.0 (default 1.0, 0 = adaptive)
--target-fps N  adaptive render-scale FPS target (default 500)
--help          show help
```

The interactive default enables vsync. Benchmark mode disables vsync unless `--vsync` is explicitly passed.

## Saves

Saves are written to `saves/world.tc` by default, or to the path passed with `--save`. The save directory is created automatically and the destination is replaced atomically through a temporary file and `rename`.

A save stores:

- The terrain seed, so the generated world can be rebuilt exactly
- Player position, yaw, and pitch
- The selected hotbar slot
- An ordered log of block edits, including negative coordinates

The current format is little-endian `TCSV` version 1. It has a 40-byte header followed by 13-byte edit records (`i32 x`, `i32 y`, `i32 z`, `u8 block`). Reads validate the magic, version, edit count, player pose, hotbar slot, and block coordinates before applying anything. Edits are replayed after chunk regeneration, which makes them survive both streaming reloads and new sessions.

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

The benchmark prints the elapsed time, total frames, average FPS, average milliseconds per frame, and the final render scale.

## Tests

```sh
make test
```

`make test` runs the portable math, chunk-mesh, world, save, player, texture, and raycast test suites.

Individual test targets:

```sh
make test-math
make test-chunk-mesh
make test-world
make test-save
make test-player
make test-texture
make test-raycast
```

The math test compares the NASM matrix multiplication against a scalar C reference and checks perspective, view matrices, and negative-coordinate floor division. The chunk test covers chunk access, solid counting in NASM, greedy merge cases, index bounds, face reduction, and a 32-build meshing benchmark. The world test covers streaming updates, negative chunk coordinates, deterministic terrain reloads, world-space block access, generation, seed stability, cave generation, per-chunk rendering bounds, neighbor-aware boundary culling, and edit replay after chunk reload. The save test covers binary round trips, negative edit coordinates, applying a save to a regenerated world, and persistence through chunk unload/reload. The player test covers gravity, ground contact, jumping, wall collision, void-floor collision, surface spawning, pitch clamping, and yaw wrapping. The texture test verifies atlas layout, tile variation, opaque pixels, and the expected grass, dirt, stone, sand, and bedrock palettes. The raycast test covers forward and negative-coordinate voxel hits, starting inside a block, placement-cell selection, misses, and reach limits. The player test also verifies that placement cells overlapping the player are rejected. The world test regenerates terrain under different seeds, checks cave placement without surface breaches, and verifies that the bedrock floor cannot be broken or replaced.

To run the math, chunk, world, save, player, texture, raycast, and optional transpiler tests:

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
src/gfx/                Window, shader, mesh, renderer, HUD, and atlas code
src/math/               Matrix math in C
src/player/             First-person movement and voxel collision
src/world/              Chunk storage, streaming world, terrain, raycasting, meshing, and saves
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
- Keep the high-resolution path fillrate-aware and avoid unnecessary overdraw.
- Keep the dependency surface small and document every external component.
- Never ship copyrighted Minecraft assets or Mojang code.

## Performance Notes

The project targets frame times that leave headroom on integrated graphics rather than merely hitting a benchmark number. The current path uses flat `uint8_t` block arrays, an inline index, a three-axis greedy mask, neighbor-aware sampling across chunk borders, frustum-culled front-to-back chunk drawing, one indexed draw per visible chunk, a nearest-filtered 2D texture array, per-vertex fog, a direct-mapped 9x9 streaming ring, reusable GPU buffers, priority remeshing after block edits, and a per-frame remesh budget. Important planned work includes:

- Chunk visibility scheduling and occlusion culling
- Batching visible chunks into fewer draw calls
- Offloading mesh generation to worker threads
- Multithreaded mesh generation
- Avoiding per-frame allocation and unnecessary copying
- Native assembly paths only for verified hotspots

The current benchmark streams 81 chunks around the player and renders roughly 33 after frustum culling. At startup the streaming ring has 439,539 solid blocks and 94,178 visible faces reduced to 17,199 quads; initial mesh building takes roughly 53 ms total on the development machine. Crossing a chunk boundary generates about 27 new chunks in roughly 7 ms and queues their remeshes under a 3 ms per-frame budget. A block edit rebuilds its chunk immediately in roughly 1 to 3 ms, while border neighbors are deferred to the next frames. Rendering averages roughly 1,100 FPS at 1280x720, 1,900 FPS at 800x450, and 580 FPS at 1920x1044 on the development machine. High-resolution performance is dominated by final framebuffer fill; optional internal render scaling is available, but the full-resolution upscale still writes every output pixel, so it is not a substitute for reducing overdraw.

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
- [x] Per-face block shading
- [x] Procedural CC0 texture atlas
- [x] Seeded procedural terrain across a chunk grid
- [x] Procedural cave generation
- [x] Dynamic chunk loading and unloading
- [x] Player controller and collision
- [x] Bedrock floor and void safety
- [x] Block picking, breaking, and placing
- [x] Frustum culling and front-to-back chunk drawing
- [x] Save and load with persistent block edits
- [x] Main and pause menus with bitmap text
- [ ] Biomes and structures
- [ ] Chunk visibility scheduling
- [ ] Multithreaded mesh building
- [ ] Portable release packaging
- [ ] Windows build investigation

## Disclaimer

ThinkCraft is an independent engine project. It is not affiliated with, endorsed by, or sponsored by Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios. ThinkCraft does not include or distribute Minecraft assets.
