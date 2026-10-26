# Assets

## Directories

- `cc0/` contains committed assets that are dedicated to the public domain under CC0 1.0.
- `local/` is ignored by git and is only for local development, experiments, or assets that cannot be redistributed.

## Rules

- Do not commit Mojang assets, extracted Minecraft files, purchased texture packs, or assets with unclear licensing.
- Include the original source and license information for every committed asset.
- Keep generated textures and temporary files out of `cc0/`.
- The engine currently requires no external image assets. Its 64x64 texture atlas is generated procedurally at startup by `src/gfx/texture.c` from original pixel data, and the generated pixel art is dedicated to CC0 1.0.
- No generated atlas image is committed.
