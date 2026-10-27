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

static void put_pixel_rgba(uint8_t *pixels, int tile, int x, int y, int red,
                           int green, int blue, int alpha) {
    int tile_x = (tile % TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    int tile_y = (tile / TC_ATLAS_GRID) * TC_ATLAS_TILE_SIZE;
    size_t index = ((size_t)(tile_y + y) * TC_ATLAS_SIZE
                    + (size_t)(tile_x + x)) * 4;

    pixels[index] = (uint8_t)red;
    pixels[index + 1] = (uint8_t)green;
    pixels[index + 2] = (uint8_t)blue;
    pixels[index + 3] = (uint8_t)alpha;
}

static void put_pixel(uint8_t *pixels, int tile, int x, int y, int red,
                      int green, int blue) {
    put_pixel_rgba(pixels, tile, x, y, red, green, blue, 255);
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

static void generate_bedrock(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, 97u);
            int red = vary_channel(54, noise, 12);
            int green = vary_channel(54, noise >> 8, 12);
            int blue = vary_channel(58, noise >> 16, 12);

            if (noise % 11u == 0) {
                red -= 22;
                green -= 22;
                blue -= 20;
            } else if (noise % 23u == 0) {
                red += 24;
                green += 24;
                blue += 22;
            }

            put_pixel(pixels, TC_TILE_BEDROCK, x, y, red, green, blue);
        }
    }
}

static const char tool_pickaxe[TC_ATLAS_TILE_SIZE][TC_ATLAS_TILE_SIZE + 1] = {
    "................",
    "................",
    ".....MMMMMM.....",
    "....MMLLLLMM....",
    "...MM..MM..MM...",
    "...M........M...",
    "................",
    "........HH......",
    ".......HH.......",
    "......HH........",
    ".....HH.........",
    "....HH..........",
    "...HH...........",
    "..HH............",
    ".HH.............",
    "................"
};

static const char tool_axe[TC_ATLAS_TILE_SIZE][TC_ATLAS_TILE_SIZE + 1] = {
    "................",
    "......MMMM......",
    ".....MLLLLM.....",
    "....MMM..MMM....",
    "....MM....MM....",
    "....MM....MM....",
    ".....MM..MM.....",
    "......MMMM......",
    ".......HH.......",
    "......HH........",
    ".....HH.........",
    "....HH..........",
    "...HH...........",
    "..HH............",
    ".HH.............",
    "................"
};

static const char tool_shovel[TC_ATLAS_TILE_SIZE][TC_ATLAS_TILE_SIZE + 1] = {
    "................",
    ".......MM.......",
    "......MLLM......",
    ".....MMMMMM.....",
    ".....MMMMMM.....",
    "......MMMM......",
    ".......HH.......",
    "......HH........",
    ".....HH.........",
    "....HH..........",
    "...HH...........",
    "..HH............",
    ".HH.............",
    "................",
    "................",
    "................"
};

static void generate_tool(uint8_t *pixels, int tile,
                          const char pattern[TC_ATLAS_TILE_SIZE]
                                         [TC_ATLAS_TILE_SIZE + 1],
                          int head_red, int head_green, int head_blue,
                          int highlight_red, int highlight_green,
                          int highlight_blue) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t noise = hash_pixel(x, y, (uint32_t)tile * 17u + 5u);
            char pixel = pattern[TC_ATLAS_TILE_SIZE - 1 - y][x];
            int red = 0;
            int green = 0;
            int blue = 0;

            if (pixel == 'H') {
                red = vary_channel(122, noise, 7);
                green = vary_channel(84, noise >> 8, 6);
                blue = vary_channel(48, noise >> 16, 5);
            } else if (pixel == 'M') {
                red = vary_channel(head_red, noise, 7);
                green = vary_channel(head_green, noise >> 8, 7);
                blue = vary_channel(head_blue, noise >> 16, 7);
            } else if (pixel == 'L') {
                red = vary_channel(highlight_red, noise, 6);
                green = vary_channel(highlight_green, noise >> 8, 6);
                blue = vary_channel(highlight_blue, noise >> 16, 6);
            } else {
                continue;
            }

            put_pixel_rgba(pixels, tile, x, y, red, green, blue, 255);
        }
    }
}

static void generate_player_tiles(uint8_t *pixels) {
    for (int y = 0; y < TC_ATLAS_TILE_SIZE; y++) {
        for (int x = 0; x < TC_ATLAS_TILE_SIZE; x++) {
            uint32_t skin_noise = hash_pixel(x, y, 131u);
            uint32_t shirt_noise = hash_pixel(x, y, 149u);
            uint32_t pants_noise = hash_pixel(x, y, 167u);
            uint32_t hair_noise = hash_pixel(x, y, 181u);

            put_pixel(pixels, TC_TILE_SKIN, x, y,
                      vary_channel(198, skin_noise, 7),
                      vary_channel(150, skin_noise >> 8, 6),
                      vary_channel(120, skin_noise >> 16, 6));
            put_pixel(pixels, TC_TILE_SHIRT, x, y,
                      vary_channel(46, shirt_noise, 8),
                      vary_channel(132, shirt_noise >> 8, 10),
                      vary_channel(160, shirt_noise >> 16, 10));
            put_pixel(pixels, TC_TILE_PANTS, x, y,
                      vary_channel(48, pants_noise, 7),
                      vary_channel(62, pants_noise >> 8, 8),
                      vary_channel(116, pants_noise >> 16, 9));
            put_pixel(pixels, TC_TILE_HAIR, x, y,
                      vary_channel(54, hair_noise, 7),
                      vary_channel(38, hair_noise >> 8, 5),
                      vary_channel(27, hair_noise >> 16, 4));
            put_pixel(pixels, TC_TILE_FACE, x, y,
                      vary_channel(198, skin_noise, 6),
                      vary_channel(150, skin_noise >> 8, 5),
                      vary_channel(120, skin_noise >> 16, 5));
        }
    }

    put_pixel(pixels, TC_TILE_FACE, 3, 11, 246, 246, 238);
    put_pixel(pixels, TC_TILE_FACE, 4, 11, 246, 246, 238);
    put_pixel(pixels, TC_TILE_FACE, 3, 12, 58, 72, 106);
    put_pixel(pixels, TC_TILE_FACE, 4, 12, 58, 72, 106);
    put_pixel(pixels, TC_TILE_FACE, 11, 11, 246, 246, 238);
    put_pixel(pixels, TC_TILE_FACE, 12, 11, 246, 246, 238);
    put_pixel(pixels, TC_TILE_FACE, 11, 12, 58, 72, 106);
    put_pixel(pixels, TC_TILE_FACE, 12, 12, 58, 72, 106);
    put_pixel(pixels, TC_TILE_FACE, 7, 6, 128, 78, 62);
    put_pixel(pixels, TC_TILE_FACE, 8, 6, 128, 78, 62);
    put_pixel(pixels, TC_TILE_FACE, 6, 7, 128, 78, 62);
    put_pixel(pixels, TC_TILE_FACE, 9, 7, 128, 78, 62);
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
    generate_bedrock(pixels);
    generate_tool(pixels, TC_TILE_WOODEN_PICKAXE, tool_pickaxe,
                  163, 118, 64, 202, 157, 96);
    generate_tool(pixels, TC_TILE_WOODEN_AXE, tool_axe,
                  163, 118, 64, 202, 157, 96);
    generate_tool(pixels, TC_TILE_WOODEN_SHOVEL, tool_shovel,
                  163, 118, 64, 202, 157, 96);
    generate_tool(pixels, TC_TILE_STONE_PICKAXE, tool_pickaxe,
                  138, 138, 148, 187, 187, 196);
    generate_tool(pixels, TC_TILE_STONE_AXE, tool_axe,
                  138, 138, 148, 187, 187, 196);
    generate_tool(pixels, TC_TILE_STONE_SHOVEL, tool_shovel,
                  138, 138, 148, 187, 187, 196);
    generate_player_tiles(pixels);
}
