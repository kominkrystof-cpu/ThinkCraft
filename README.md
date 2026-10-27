# ThinkCraft

ThinkCraft is a lightweight, Minecraft-like engine written in C11 and x86-64 NASM. It uses OpenGL 3.3 core through GLFW and Glad, with a focus on predictable performance, a small dependency surface, and assembly hot paths.

The project is currently an engine foundation with a playable first- and third-person demo, not a full Minecraft clone. The current milestone streams a 9x9 ring (81 chunks) of 16x256x16 voxel chunks around the player, generates domain-warped Perlin terrain with ridged mountains and 3D cave systems, meshes each chunk with neighbor-aware greedy meshing, and renders it with a procedural pixel-art texture atlas, a swept-AABB controller, a blocky player model with held items, block breaking and placing, survival stats, an unbreakable bedrock floor, distance fog, a main/pause menu, and persistent world saves.

## Status

- Platform: Linux x86-64, X11 or XWayland
- Graphics: OpenGL 3.3 core, GLFW 3, Glad
- Language: C11 with x86-64 NASM
- Runtime: no JVM, no garbage collector, no engine scripting runtime
- Target hardware: Intel UHD 620 / i3-10110U-class laptops
- Target: stable 1000 FPS at 1920x1044 with vsync disabled

The current streaming-world benchmark runs at roughly 1,250 FPS at 1280x720 and 560 FPS at 1920x1044 on the development Intel UHD 620 machine with vsync disabled. It renders a 9x9 ring around the player, rejects chunks outside the frustum or beyond the fog distance (roughly 33 visible chunks at spawn), draws visible chunks front-to-back, spreads boundary remeshing across frames when the player crosses a chunk edge, and uses distance fog to hide the streaming boundary.

## Current Milestone: Persistent Worlds, Terrain, and Rendering

This milestone combines durable world state with a new terrain generator, swept collision, and a compact render path. ThinkCraft boots into a main menu, supports New World and Continue, exposes a pause menu, renders UI text without external assets, and stores seeds, player pose, hotbar selection, and block edits in a versioned binary save. Terrain is generated from hash-based 2D/3D Perlin noise with domain warping, ridged mountains, beaches, and caves. Movement uses swept AABB contact resolution with 12-step bisection and 0.125-block substeps. Chunk meshes use packed 10-byte vertices and a per-chunk origin uniform.

- 1920x1044 OpenGL 3.3 core window (default, configurable)
- GLFW window creation, key-state input, mouse capture, and clean shutdown
- Glad-generated OpenGL loader
- Shader loading and compilation with error reporting
- Depth testing, back-face culling, and softened distance fog (64 to 80 blocks)
- 16x256x16 chunk storage with inline bounds-checked block access
- Dynamic 9x9 chunk streaming ring around the player
- Seeded 2D Perlin height fields with domain warping, ridged mountains, beaches, and stone peaks
- 3D Perlin cave systems carved beneath the surface with no surface breaches
- Negative world and chunk coordinates through floor division
- Automatic load/unload of chunk rows and columns as the player moves
- Budgeted dirty-slot remeshing with priority edits and deferred neighbor updates
- Neighbor-aware greedy meshing across all three axes with internal-face culling
- Initial world reduced from 106,078 visible faces to 26,780 quads (3.96x)
- 81 per-chunk textured meshes uploaded to VBOs and EBOs with packed 10-byte vertices
- Frustum culling, fog-distance chunk rejection, and front-to-back drawing (roughly 33 of 81 chunks visible at spawn)
- Procedural 128x128 RGBA atlas uploaded as a 17-layer 2D texture array of 16x16 tiles
- Textured grass, dirt, stone, sand, and bedrock with per-face shading
- Procedural tool icons plus skin, shirt, pants, hair, and face tiles
- Blocky player model, first-person arm, and held block/tool rendering
- First-person and collision-aware third-person cameras cycled with F5
- Per-vertex distance fog and optional internal render scaling
- Repeating tile-local UVs on greedy-merged quads
- First-person WASD controller with mouse look, Ctrl sprint, and Shift crouch
- DDA voxel picking with a six-block reach
- Hold left to mine with tool speed; right click places with collision checks
- Nine-slot hotbar and a 27-slot backpack with click, split, and quick-move controls
- Survival, Creative, and Spectator modes with creative flight and spectator noclip
- Survival health, hunger, exhaustion, regeneration, starvation, fall damage, and a death/respawn screen
- Hearts and hunger HUD drawn through a batched OpenGL quad path
- Crouch edge protection that stops Shift-crouching players from walking off blocks
- Item registry with stack limits, tool durability, block hardness, and mining drops
- Gravity, jumping, swept AABB voxel collision, one-block auto-step, and surface spawning
- Unbreakable bedrock floor at y=0 and a hard void-floor collision clamp
- Procedural stone, grass, dirt, and sand terrain with seeded plains, hills, mountains, beaches, and caves
- Main menu with New World, Continue, and Quit
- Pause menu with Resume, Save, Load, and Quit to Menu
- Persistent binary saves containing the seed, player pose, game mode, full inventory, survival stats, and block edits
- Built-in 5x7 uppercase bitmap text renderer for menu UI and status messages
- Arena allocator for startup allocations
- 4x4 matrix math in C, including `look_at`
- `tc_mat4_mul` implemented in x86-64 NASM
- `tc_chunk_count_solid` implemented in x86-64 NASM
- `tc_asm_aabb_collides` implemented in x86-64 NASM for the player collision hot path
- Frame-time and FPS reporting
- Automated render benchmark and mesh benchmark output
- Chunk, world, save, player, texture, raycast, and mesh tests covering
  boundary culling, streaming, generation determinism, seed stability, cave
  generation, bedrock protection, void-floor collision, negative
  coordinates, swept collision, high-speed movement, ceilings, auto-step,
  diagonal sliding, look limits, atlas tile data, voxel picking, placement
  overlap, save round trips, save-version rejection, and edit persistence
  across chunk reloads, survival statistics, crouch edge protection, fall
  damage, and NASM collision bounds

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
Left click    capture the mouse; hold to mine the targeted block
Right click   place the selected block
WASD          move; navigate menus when paused
Mouse         look around
Space         jump; fly up in Creative/Spectator; activate menu item; respawn when dead
Shift         crouch in Survival (with block-edge protection); fly down in Creative/Spectator; quick move in inventory
Ctrl          sprint in Survival; fly down in Creative/Spectator
1-9           select a hotbar slot
E             open/close the inventory
F3            toggle the debug overlay
F4            cycle Survival, Creative, and Spectator
F5            cycle first-person, third-person back, and third-person front cameras
F6            quick save while playing or paused
F9            quick load while playing or paused
Escape        close inventory; release the mouse; pause; quit from the main menu
Up/Down       navigate menus
Enter/Space   activate menu item
```

The game starts at the main menu. New World regenerates the world from the configured seed, Continue loads the save at the configured path, and Quit exits. The pause menu can save or load without leaving the game. Saves default to `saves/world.tc`.

Survival mode shows ten hearts and ten hunger icons above the hotbar. Sprinting, walking, jumping, and mining build exhaustion; exhaustion drains hunger, full hunger slowly regenerates health, and an empty hunger bar causes starvation damage. Falling more than three blocks deals damage on landing. Death shows a red death screen; press Space or Enter to respawn at the surface with full health and hunger. Creative and Spectator modes do not consume hunger or take fall damage.

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
--render-distance N  chunk radius 2-6 (default 4)
--fps-cap N     frame rate cap, 0 = uncapped (default 0)
--sensitivity N mouse sensitivity (default 0.0022)
--config PATH   settings file path
--help          show help
```

The interactive default enables vsync. Benchmark mode disables vsync unless `--vsync` is explicitly passed.

## Settings

Settings are read from `$XDG_CONFIG_HOME/thinkcraft/config.cfg`, falling back to `~/.config/thinkcraft/config.cfg`. Pass `--config PATH` to use a different file. The file is a simple `key=value` list with `#` comments:

```text
render_distance=4
vsync=1
fps_cap=0
render_scale=1.0
target_fps=500
sensitivity=0.0022
```

Command-line options override the file. Settings are written back on a normal exit using an atomic temporary file and `rename`.

## Saves

Saves are written to `saves/world.tc` by default, or to the path passed with `--save`. The save directory is created automatically and the destination is replaced atomically through a temporary file and `rename`.

A save stores:

- The terrain seed, so the generated world can be rebuilt exactly
- Player position, yaw, and pitch
- The selected hotbar slot and the full 36-slot inventory, including the held cursor stack
- The active game mode (Survival, Creative, or Spectator)
- Survival health, hunger, and exhaustion
- An ordered log of block edits, including negative coordinates

The current format is little-endian `TCSV` version 4. It has a 64-byte header followed by 36 eight-byte inventory records, one eight-byte cursor record, and 13-byte edit records (`i32 x`, `i32 y`, `i32 z`, `u8 block`). Inventory records store `u16 item`, `u16 count`, `u16 durability`, and a reserved `u16`. The version 4 header stores health at offset 48, hunger at 52, exhaustion at 56, and a reserved word at 60. Reads validate the magic, version, edit count, player pose, game mode, survival ranges, every item stack, and block coordinates before applying anything. Version 3 saves are accepted with full health and hunger defaults, so existing worlds load unchanged. Version 1 and 2 saves are rejected. Edits are replayed after chunk regeneration, which makes them survive both streaming reloads and new sessions.

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

`make test` runs the portable math, chunk-mesh, world, save, player, texture, raycast, inventory, and survival test suites.

Individual test targets:

```sh
make test-math
make test-chunk-mesh
make test-world
make test-save
make test-player
make test-texture
make test-raycast
make test-inventory
make test-survival
```

The math test compares the NASM matrix multiplication against a scalar C reference and checks perspective, view matrices, and negative-coordinate floor division. The chunk test covers chunk access, solid counting in NASM, greedy merge cases, index bounds, face reduction, packed local positions, mesh origins, and a 32-build meshing benchmark. The world test covers streaming updates, negative chunk coordinates, deterministic Perlin terrain reloads, world-space block access, seed stability, cave generation without surface breaches, per-chunk rendering bounds, neighbor-aware boundary culling, and edit replay after chunk reload. The save test covers binary round trips, inventory and game-mode persistence, survival-stat persistence, version 3 compatibility with full-health defaults, negative edit coordinates, applying a save to a regenerated world, persistence through chunk unload/reload, and rejection of version 2 saves. The player test covers gravity, ground contact, jumping, wall collision, high-speed tunneling, ceiling contact, one-block auto-step, negative-coordinate walls, diagonal wall sliding, void-floor collision, surface spawning, pitch clamping, yaw wrapping, creative flight, spectator noclip, crouch edge protection, fall damage, and NASM collision bounds across negative and chunk-edge coordinates. The survival test covers damage and healing caps, death, fall damage, exhaustion, regeneration, and starvation. The inventory test covers item stats, stacking, overflow, splitting, quick move, consumption, and tool durability. The texture test verifies atlas layout, tile variation, opaque terrain and player tiles, transparent tool backgrounds, face details, and the expected grass, dirt, stone, sand, and bedrock palettes. The raycast test covers forward and negative-coordinate voxel hits, hit distances, starting inside a block, placement-cell selection, misses, and reach limits. The player test also verifies that placement cells overlapping the player are rejected. The world test regenerates terrain under different seeds and verifies that the bedrock floor cannot be broken or replaced.

To run all test suites, including inventory, survival, and the optional transpiler tests:

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
src/player/             First-person movement, crouch edge protection, and voxel collision
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

The project targets frame times that leave headroom on integrated graphics rather than merely hitting a benchmark number. The current path uses flat `uint8_t` block arrays, an inline index, a three-axis greedy mask, neighbor-aware sampling across chunk borders, frustum culling, fog-distance chunk rejection, front-to-back drawing with early depth rejection, one indexed draw per visible chunk, packed 10-byte vertices, a per-chunk origin uniform, a nearest-filtered 2D texture array, per-vertex fog, reusable VAO/VBO/EBO state across draws, a direct-mapped 9x9 streaming ring, priority remeshing after block edits, and a per-frame remesh budget. Important planned work includes:

- Occlusion culling and more precise chunk visibility scheduling
- Batching visible chunks into fewer draw calls
- Worker-thread and multithreaded mesh generation
- Larger render distances with level-of-detail terrain
- Native assembly paths only for verified hotspots

The current benchmark streams 81 chunks around the player and renders roughly 33 after frustum and fog-distance culling. At startup the streaming ring has 281,328 solid blocks and 106,078 visible faces reduced to 26,780 quads; initial mesh building takes roughly 38 to 40 ms total on the development machine. New chunks are generated and remeshed under a 3 ms per-frame budget. A block edit rebuilds its chunk immediately, while border neighbors are deferred to the next frames. Rendering averages roughly 1,250 FPS at 1280x720, 2,200 FPS at 800x450, and 560 FPS at 1920x1044 on the development machine. The 1920x1044 path is fillrate-bound and remains below the 1000 FPS target. High-resolution performance is dominated by final framebuffer fill; optional internal render scaling is available, but the full-resolution upscale still writes every output pixel, so it is not a substitute for reducing overdraw.

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
- [x] Domain-warped Perlin terrain with ridged mountains and caves
- [x] Swept AABB collision with high-speed and ceiling coverage
- [x] Packed chunk vertices and fog-distance chunk rejection
- [x] Settings file, runtime render distance, and F3 debug overlay
- [x] Item registry, inventory model, tools, durability, and mining drops
- [x] Survival, Creative, and Spectator modes
- [x] Survival health, hunger, fall damage, regeneration, and starvation
- [x] Crouch block-edge protection
- [x] Batched HUD quad rendering
- [x] NASM AABB collision hot path
- [ ] Biomes and structures
- [ ] Occlusion culling and chunk visibility scheduling
- [ ] Batched multi-chunk drawing
- [ ] Multithreaded mesh building
- [ ] Larger render distance with level-of-detail terrain
- [ ] Portable release packaging
- [ ] Windows build investigation

## Disclaimer

ThinkCraft is an independent engine project. It is not affiliated with, endorsed by, or sponsored by Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios. ThinkCraft does not include or distribute Minecraft assets.
