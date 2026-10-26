#include "thinkcraft/texture.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_texture: %s\n", message);
    }
    return condition;
}

static const uint8_t *tile_pixel(const uint8_t *pixels, int tile, int x, int y) {
    int tile_x = (tile % TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    int tile_y = (tile / TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    size_t index = ((size_t)(tile_y + y) * TC_ATLAS_SIZE
                    + (size_t)(tile_x + x)) * 4;

    return pixels + index;
}

static int tile_color_count(const uint8_t *pixels, int tile) {
    uint32_t colors[TC_ATLAS_TILE_SIZE * TC_ATLAS_TILE_SIZE];
    int count = 0;

    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            const uint8_t *pixel = tile_pixel(pixels, tile, x, y);
            uint32_t color = (uint32_t)pixel[0] << 16
                           | (uint32_t)pixel[1] << 8
                           | (uint32_t)pixel[2];
            int found = 0;

            for (int i = 0; i < count; i++) {
                if (colors[i] == color) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                colors[count++] = color;
            }
        }
    }

    return count;
}

static int tile_has_green(const uint8_t *pixels, int tile) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            const uint8_t *pixel = tile_pixel(pixels, tile, x, y);

            if (pixel[1] > pixel[0] + 30 && pixel[1] > pixel[2] + 30) {
                return 1;
            }
        }
    }
    return 0;
}

static int test_layout(void) {
    return check(TC_TILE_COUNT <= TC_ATLAS_GRID * TC_ATLAS_GRID,
                 "too many tiles for atlas")
        && check(TC_ATLAS_PIXEL_COUNT
                 == TC_ATLAS_SIZE * TC_ATLAS_SIZE * 4,
                 "atlas size mismatch");
}

static int test_generated_atlas(void) {
    uint8_t pixels[TC_ATLAS_PIXEL_COUNT];
    int used_tiles[] = {
        TC_TILE_GRASS_TOP,
        TC_TILE_GRASS_SIDE,
        TC_TILE_DIRT,
        TC_TILE_STONE,
        TC_TILE_SAND
    };

    tc_texture_generate_atlas(pixels);

    for (size_t i = 0; i < sizeof(used_tiles) / sizeof(used_tiles[0]); i++) {
        int tile = used_tiles[i];

        if (!check(tile_color_count(pixels, tile) >= 16,
                   "tile lacks variation")) {
            return 0;
        }
        for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
            for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
                const uint8_t *pixel = tile_pixel(pixels, tile, x, y);

                if (!check(pixel[3] == 255, "tile alpha is not opaque")) {
                    return 0;
                }
            }
        }
    }

    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            const uint8_t *pixel = tile_pixel(pixels, TC_TILE_GRASS_TOP, x, y);

            if (!check(pixel[1] > pixel[0] && pixel[1] > pixel[2],
                       "grass top is not green")) {
                return 0;
            }
        }
    }

    const uint8_t *grass_side_top =
        tile_pixel(pixels, TC_TILE_GRASS_SIDE, 8, TC_ATLAS_TILE_SIZE - 1);
    const uint8_t *grass_side_bottom =
        tile_pixel(pixels, TC_TILE_GRASS_SIDE, 8, 0);

    if (!check(tile_has_green(pixels, TC_TILE_GRASS_SIDE),
               "grass side has no green")
        || !check(grass_side_top[1] > grass_side_top[0]
                  && grass_side_top[1] > grass_side_top[2],
                  "grass side green edge is not at the top")
        || !check(grass_side_bottom[0] > grass_side_bottom[1]
                  && grass_side_bottom[1] > grass_side_bottom[2],
                  "grass side bottom is not dirt")) {
        return 0;
    }

    const uint8_t *dirt = tile_pixel(pixels, TC_TILE_DIRT, 4, 4);
    const uint8_t *stone = tile_pixel(pixels, TC_TILE_STONE, 4, 4);
    const uint8_t *sand = tile_pixel(pixels, TC_TILE_SAND, 4, 4);
    int stone_spread = (int)stone[0] - (int)stone[2];

    if (!check(dirt[0] > dirt[1] && dirt[1] > dirt[2],
               "dirt color is not brown")
        || !check(stone_spread < 40 && stone_spread > -40,
                  "stone color is not gray")
        || !check(sand[0] > sand[1] && sand[1] > sand[2],
                  "sand color is not warm")) {
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_layout() || !test_generated_atlas()) {
        return 1;
    }

    printf("test_texture: ok\n");
    return 0;
}
