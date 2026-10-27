#include "thinkcraft/terrain.h"

#include <math.h>

#define TC_PERLIN_2D_SCALE 1.41421356f
#define TC_PERLIN_3D_SCALE 1.15470054f

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

static float fade(float value) {
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static float smoothstep(float edge0, float edge1, float value) {
    float t = clamp01((value - edge0) / (edge1 - edge0));

    return t * t * (3.0f - 2.0f * t);
}

static float grad2(uint32_t hash, float x, float z) {
    const float diagonal = 0.70710678f;

    switch (hash & 7u) {
        case 0:
            return x;
        case 1:
            return -x;
        case 2:
            return z;
        case 3:
            return -z;
        case 4:
            return (x + z) * diagonal;
        case 5:
            return (-x + z) * diagonal;
        case 6:
            return (x - z) * diagonal;
        default:
            return (-x - z) * diagonal;
    }
}

static float grad3(uint32_t hash, float x, float y, float z) {
    switch (hash % 12u) {
        case 0:
            return x + y;
        case 1:
            return -x + y;
        case 2:
            return x - y;
        case 3:
            return -x - y;
        case 4:
            return x + z;
        case 5:
            return -x + z;
        case 6:
            return x - z;
        case 7:
            return -x - z;
        case 8:
            return y + z;
        case 9:
            return -y + z;
        case 10:
            return y - z;
        default:
            return -y - z;
    }
}

static float perlin2(float x, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int z0 = (int)floorf(z);
    float fx = x - (float)x0;
    float fz = z - (float)z0;
    float u = fade(fx);
    float v = fade(fz);
    float n00 = grad2(hash2(x0, z0, seed), fx, fz);
    float n10 = grad2(hash2(x0 + 1, z0, seed), fx - 1.0f, fz);
    float n01 = grad2(hash2(x0, z0 + 1, seed), fx, fz - 1.0f);
    float n11 = grad2(hash2(x0 + 1, z0 + 1, seed), fx - 1.0f, fz - 1.0f);
    float value = lerp(lerp(n00, n10, u), lerp(n01, n11, u), v);

    value *= TC_PERLIN_2D_SCALE;
    return clamp01(value * 0.5f + 0.5f);
}

static float perlin3(float x, float y, float z, uint32_t seed) {
    int x0 = (int)floorf(x);
    int y0 = (int)floorf(y);
    int z0 = (int)floorf(z);
    float fx = x - (float)x0;
    float fy = y - (float)y0;
    float fz = z - (float)z0;
    float u = fade(fx);
    float v = fade(fy);
    float w = fade(fz);
    float n000 = grad3(hash3(x0, y0, z0, seed), fx, fy, fz);
    float n100 = grad3(hash3(x0 + 1, y0, z0, seed), fx - 1.0f, fy, fz);
    float n010 = grad3(hash3(x0, y0 + 1, z0, seed), fx, fy - 1.0f, fz);
    float n110 = grad3(hash3(x0 + 1, y0 + 1, z0, seed), fx - 1.0f,
                       fy - 1.0f, fz);
    float n001 = grad3(hash3(x0, y0, z0 + 1, seed), fx, fy, fz - 1.0f);
    float n101 = grad3(hash3(x0 + 1, y0, z0 + 1, seed), fx - 1.0f, fy,
                       fz - 1.0f);
    float n011 = grad3(hash3(x0, y0 + 1, z0 + 1, seed), fx, fy - 1.0f,
                       fz - 1.0f);
    float n111 = grad3(hash3(x0 + 1, y0 + 1, z0 + 1, seed), fx - 1.0f,
                       fy - 1.0f, fz - 1.0f);
    float x00 = lerp(n000, n100, u);
    float x10 = lerp(n010, n110, u);
    float x01 = lerp(n001, n101, u);
    float x11 = lerp(n011, n111, u);
    float value = lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);

    value *= TC_PERLIN_3D_SCALE;
    return clamp01(value * 0.5f + 0.5f);
}

static float fbm2(float x, float z, uint32_t seed, int octaves) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < octaves; octave++) {
        sum += perlin2(x * frequency, z * frequency,
                       seed + (uint32_t)octave * 0x9e3779b9u)
             * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static float fbm3(float x, float y, float z, uint32_t seed, int octaves) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < octaves; octave++) {
        sum += perlin3(x * frequency, y * frequency, z * frequency,
                       seed + (uint32_t)octave * 0x7f4a7c15u)
             * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static float ridged2(float x, float z, uint32_t seed, int octaves) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < octaves; octave++) {
        float value = perlin2(x * frequency, z * frequency,
                              seed + (uint32_t)octave * 0x85ebca77u);

        value = 1.0f - fabsf(value * 2.0f - 1.0f);
        sum += value * value * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static float ridged3(float x, float y, float z, uint32_t seed, int octaves) {
    float sum = 0.0f;
    float weight = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0; octave < octaves; octave++) {
        float value = perlin3(x * frequency, y * frequency, z * frequency,
                              seed + (uint32_t)octave * 0x27d4eb2fu);

        value = 1.0f - fabsf(value * 2.0f - 1.0f);
        sum += value * value * amplitude;
        weight += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return sum / weight;
}

static int cave_at(int x, int y, int z, uint32_t seed) {
    float px = (float)x;
    float py = (float)y;
    float pz = (float)z;
    float cavern = fbm3(px * 0.031f, py * 0.058f, pz * 0.031f,
                        seed ^ 0xc2b2ae35u, 3);
    float tunnel_a = ridged3(px * 0.043f, py * 0.094f, pz * 0.043f,
                             seed ^ 0x27d4eb2fu, 2);
    float tunnel_b = ridged3(px * 0.043f + 31.7f, py * 0.094f - 17.3f,
                             pz * 0.043f + 11.9f,
                             seed ^ 0x165667b1u, 1);
    float tunnel = tunnel_a * tunnel_b;

    return (cavern > 0.59f && tunnel > 0.30f) || tunnel > 0.62f;
}

int tc_terrain_height(int x, int z, uint32_t seed) {
    float px = (float)x;
    float pz = (float)z;
    float warp_x = (perlin2(px * 0.0045f + 19.7f, pz * 0.0045f - 13.1f,
                            seed ^ 0xa511e9b3u) - 0.5f) * 48.0f;
    float warp_z = (perlin2(px * 0.0045f - 23.9f, pz * 0.0045f + 31.3f,
                            seed ^ 0x63d83595u) - 0.5f) * 48.0f;
    float wx = px + warp_x;
    float wz = pz + warp_z;
    float continent = fbm2(wx * 0.0032f, wz * 0.0032f,
                           seed ^ 0x9e3779b9u, 5);
    float hills = fbm2(wx * 0.018f, wz * 0.018f,
                       seed ^ 0x85ebca77u, 4);
    float ridge = ridged2(wx * 0.008f, wz * 0.008f,
                          seed ^ 0xc2b2ae35u, 3);
    float mountain = smoothstep(0.56f, 0.82f, continent) * ridge;
    float height = 6.0f + continent * 18.0f + (hills - 0.5f) * 8.0f
                 + mountain * 14.0f;
    int result = (int)floorf(height);

    if (result < 6) {
        result = 6;
    }
    if (result > 48) {
        result = 48;
    }

    return result;
}

static uint8_t surface_block(int x, int z, int height, uint32_t seed) {
    float beach = perlin2((float)x * 0.028f, (float)z * 0.028f,
                          seed ^ 0x9e3779b9u);
    float rock = fbm2((float)x * 0.012f, (float)z * 0.012f,
                      seed ^ 0x7f4a7c15u, 2);

    if (height >= 34 || (height >= 24 && rock > 0.68f)) {
        return TC_BLOCK_STONE;
    }
    if (height <= 11 && beach > 0.55f) {
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
