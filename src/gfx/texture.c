#include "thinkcraft/texture.h"

#include <string.h>

static uint32_t hash_pixel(int x, int y, uint32_t seed) {
    uint32_t value = (uint32_t)x * 374761393u
                   + (uint32_t)y * 668265263u
                   + seed * 2246822519u;

    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}

static uint8_t clamp_channel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static uint8_t vary_channel(int base, uint32_t noise, int radius) {
    int offset = (int)(noise % (uint32_t)(radius * 2 + 1)) - radius;

    return clamp_channel(base + offset);
}

static void put_pixel(uint8_t *pixels, int tile, int x, int y, int red,
                      int green, int blue) {
    int tile_x = (tile % TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    int tile_y = (tile / TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    size_t index = ((size_t)(tile_y + y) * TC_ATLAS_SIZE
                    + (size_t)(tile_x + x)) * 4;

    pixels[index] = (uint8_t)red;
    pixels[index + 1] = (uint8_t)green;
    pixels[index + 2] = (uint8_t)blue;
    pixels[index + 3] = 255;
}

static void generate_grass_top(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, 11u);
            int red = vary_channel(82, noise, 10);
            int green = vary_channel(157, noise >> 8, 16);
            int blue = vary_channel(52, noise >> 16, 8);

            if (noise % 19u == 0) {
                red -= 10;
                green -= 18;
                blue -= 6;
            } else if (noise % 29u == 0) {
                red += 8;
                green += 12;
                blue += 5;
            }

            put_pixel(pixels, TC_TILE_GRASS_TOP, x, y, red, green, blue);
        }
    }
}

static void generate_grass_side(uint8_t *pixels) {
    for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
        int depth = 3 + (int)(hash_pixel(x, 0, 31u) % 3u);

        for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
            uint32_t noise = hash_pixel(x, y, 41u);
            int green_edge = TC_ATLAS_TILE_SIZE - depth;
            int red;
            int green;
            int blue;

            if (y >= green_edge) {
                red = vary_channel(79, noise, 9);
                green = vary_channel(151, noise >> 8, 14);
                blue = vary_channel(49, noise >> 16, 7);

                if (y == TC_ATLAS_TILE_SIZE - 1) {
                    green += 10;
                }
            } else if (y == green_edge - 1 && noise % 5u == 0) {
                red = vary_channel(78, noise, 8);
                green = vary_channel(143, noise >> 8, 10);
                blue = vary_channel(49, noise >> 16, 6);
            } else {
                red = vary_channel(132, noise, 11);
                green = vary_channel(95, noise >> 8, 9);
                blue = vary_channel(66, noise >> 16, 7);

                if (noise % 23u == 0) {
                    red -= 14;
                    green -= 11;
                    blue -= 8;
                }
            }

            put_pixel(pixels, TC_TILE_GRASS_SIDE, x, y, red, green, blue);
        }
    }
}

static void generate_dirt(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, 53u);
            int red = vary_channel(132, noise, 11);
            int green = vary_channel(95, noise >> 8, 9);
            int blue = vary_channel(66, noise >> 16, 7);

            if (noise % 23u == 0) {
                red -= 15;
                green -= 11;
                blue -= 8;
            } else if (noise % 31u == 0) {
                red += 12;
                green += 9;
                blue += 6;
            }

            put_pixel(pixels, TC_TILE_DIRT, x, y, red, green, blue);
        }
    }
}

static void generate_stone(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, 67u);
            int red = vary_channel(126, noise, 9);
            int green = vary_channel(126, noise >> 8, 9);
            int blue = vary_channel(132, noise >> 16, 9);

            if ((x + y * 3 + (int)(noise % 5u)) % 17 == 0) {
                red -= 17;
                green -= 17;
                blue -= 15;
            } else if (noise % 37u == 0) {
                red += 13;
                green += 13;
                blue += 11;
            }

            put_pixel(pixels, TC_TILE_STONE, x, y, red, green, blue);
        }
    }
}

static void generate_sand(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, 79u);
            int red = vary_channel(219, noise, 7);
            int green = vary_channel(205, noise >> 8, 6);
            int blue = vary_channel(157, noise >> 16, 6);

            if (noise % 29u == 0) {
                red -= 12;
                green -= 10;
                blue -= 8;
            } else if (noise % 41u == 0) {
                red += 7;
                green += 6;
                blue += 5;
            }

            put_pixel(pixels, TC_TILE_SAND, x, y, red, green, blue);
        }
    }
}

void tc_texture_generate_atlas(uint8_t *pixels) {
    if (pixels == NULL) {
        return;
    }

    memset(pixels, 0, TC_ATLAS_PIXEL_COUNT);
    generate_grass_top(pixels);
    generate_grass_side(pixels);
    generate_dirt(pixels);
    generate_stone(pixels);
    generate_sand(pixels);
}
