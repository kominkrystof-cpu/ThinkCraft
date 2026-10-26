#include "thinkcraft/terrain.h"

#include <math.h>

static uint32_t mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t value = seed;
    value ^= (uint32_t)x * 0x9e3779b1u;
    value ^= (uint32_t)z * 0x85ebca77u;
    return mix32(value);
}

static uint32_t hash3(int x, int y, int z, uint32_t seed) {
    uint32_t value = seed;
    value ^= (uint32_t)x * 0x9e3779b1u;
    value ^= (uint32_t)y * 0xc2b2ae35u;
    value ^= (uint32_t)z * 0x85ebca77u;
    return mix32(value);
}

static float unit_float(uint32_t value) {
    return (float)(value & 0x00ffffffu) * (1.0f / 16777215.0f);
}

static float smooth_curve(float value) {
    return value * value * (3.0f - 2.0f * value);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float value_noise2(float x, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    float tx = smooth_curve(x - (float)x0);
    float tz = smooth_curve(z - (float)z0);
    float n00 = unit_float(hash2(x0, z0, seed));
    float n10 = unit_float(hash2(x0 + 1, z0, seed));
    float n01 = unit_float(hash2(x0, z0 + 1, seed));
    float n11 = unit_float(hash2(x0 + 1, z0 + 1, seed));

    return lerp(lerp(n00, n10, tx), lerp(n01, n11, tx), tz);
}

static float value_noise3(float x, float y, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int z0 = (int)floorf(z);
    float tx = smooth_curve(x - (float)x0);
    float ty = smooth_curve(y - (float)y0);
    float tz = smooth_curve(z - (float)z0);
    float n000 = unit_float(hash3(x0, y0, z0, seed));
    float n100 = unit_float(hash3(x0 + 1, y0, z0, seed));
    float n010 = unit_float(hash3(x0, y0 + 1, z0, seed));
    float n110 = unit_float(hash3(x0 + 1, y0 + 1, z0, seed));
    float n001 = unit_float(hash3(x0, y0, z0 + 1, seed));
    float n101 = unit_float(hash3(x0 + 1, y0, z0 + 1, seed));
    float n011 = unit_float(hash3(x0, y0 + 1, z0 + 1, seed));
    float n111 = unit_float(hash3(x0 + 1, y0 + 1, z0 + 1, seed));
    float x00 = lerp(n000, n100, tx);
    float x10 = lerp(n010, n110, tx);
    float x01 = lerp(n001, n101, tx);
    float x11 = lerp(n011, n111, tx);

    return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
}

static float fbm2(float x, float z, uint32_t seed) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < 4; octave++) {
        sum += value_noise2(x * frequency, z * frequency,
                            seed + (uint32_t)octave * 0x9e3779b9u)
             * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static float fbm3(float x, float y, float z, uint32_t seed) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < 2; octave++) {
        sum += value_noise3(x * frequency, y * frequency, z * frequency,
                            seed + (uint32_t)octave * 0x7f4a7c15u)
             * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static int cave_at(int x, int y, int z, uint32_t seed) {
    float cavern = fbm3((float)x * 0.045f, (float)y * 0.070f,
                        (float)z * 0.045f, seed ^ 0xc2b2ae35u);
    float tunnel = value_noise3((float)x * 0.095f, (float)y * 0.140f,
                                (float)z * 0.095f, seed ^ 0x27d4eb2fu);

    return cavern > 0.62f && tunnel > 0.42f;
}

int tc_terrain_height(int x, int z, uint32_t seed) {
    float broad = fbm2((float)x * 0.006f, (float)z * 0.006f,
                       seed ^ 0xa511e9b3u);
    float detail = fbm2((float)x * 0.028f, (float)z * 0.028f,
                        seed ^ 0x63d83595u);
    int height = 9 + (int)(broad * 19.0f + detail * 7.0f);

    if (height < 6) {
        height = 6;
    }
    if (height > 44) {
        height = 44;
    }

    return height;
}

static uint8_t surface_block(int x, int z, int height, uint32_t seed) {
    float sand = value_noise2((float)x * 0.032f, (float)z * 0.032f,
                              seed ^ 0x9e3779b9u);

    if (height >= 31) {
        return TC_BLOCK_STONE;
    }
    if (height <= 11 && sand > 0.58f) {
        return TC_BLOCK_SAND;
    }

    return TC_BLOCK_GRASS;
}

void tc_terrain_generate_chunk(tc_chunk *chunk, uint32_t seed) {
    if (chunk == NULL) {
        return;
    }

    tc_chunk_clear(chunk);

    for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
        for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
            int world_x = chunk->origin_x + x;
            int world_z = chunk->origin_z + z;
            int height = tc_terrain_height(world_x, world_z, seed);
            uint8_t top = surface_block(world_x, world_z, height, seed);

            for (int y = 0; y <= height; y++) {
                uint8_t block;

                if (y < TC_WORLD_BEDROCK_LAYERS) {
                    block = TC_BLOCK_BEDROCK;
                } else if (y == height) {
                    block = top;
                } else if (top == TC_BLOCK_STONE) {
                    block = TC_BLOCK_STONE;
                } else if (top == TC_BLOCK_SAND && y >= height - 2) {
                    block = TC_BLOCK_SAND;
                } else if (y >= height - 3) {
                    block = TC_BLOCK_DIRT;
                } else {
                    block = TC_BLOCK_STONE;
                }

                tc_chunk_set(chunk, x, y, z, block);
            }

            for (int y = 3; y <= height - 4; y++) {
                if (cave_at(world_x, y, world_z, seed)) {
                    tc_chunk_set(chunk, x, y, z, TC_BLOCK_AIR);
                }
            }
        }
    }

    chunk->solid_count = tc_chunk_count_solid(chunk->blocks,
                                              TC_CHUNK_BLOCK_COUNT);
}
