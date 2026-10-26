#include "thinkcraft/arena.h"
#include "thinkcraft/math.h"
#include "thinkcraft/player.h"
#include "thinkcraft/save.h"
#include "thinkcraft/terrain.h"
#include "thinkcraft/world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_save: %s\n", message);
    }
    return condition;
}

static int init_world(tc_arena *arena, tc_world *world, uint32_t seed) {
    if (!tc_arena_init(arena, 4 * 1024 * 1024)) {
        return 0;
    }
    if (!tc_world_init(world, arena, 1)) {
        tc_arena_shutdown(arena);
        return 0;
    }
    tc_world_set_seed(world, seed);
    tc_world_generate_terrain(world);
    return 1;
}

static int apply_save(tc_arena *arena, tc_world *world,
                      const tc_save_data *data) {
    int chunk_x;
    int chunk_z;

    if (!tc_arena_init(arena, 4 * 1024 * 1024)) {
        return 0;
    }
    if (!tc_world_init(world, arena, 1)) {
        tc_arena_shutdown(arena);
        return 0;
    }

    tc_world_set_seed(world, data->seed);
    chunk_x = tc_math_floor_div((int)floorf(data->position[0]),
                                TC_CHUNK_SIZE_X);
    chunk_z = tc_math_floor_div((int)floorf(data->position[2]),
                                TC_CHUNK_SIZE_Z);
    if (!tc_world_update(world, chunk_x, chunk_z)) {
        return 0;
    }

    tc_world_generate_terrain(world);

    for (size_t index = 0; index < data->edit_count; index++) {
        const tc_world_edit *edit = &data->edits[index];

        if (!tc_world_add_edit(world, edit->x, edit->y, edit->z,
                               edit->block)) {
            return 0;
        }
    }

    return 1;
}

int main(void) {
    char directory[256];
    char path[512];
    tc_arena arena;
    tc_arena restored_arena = {0};
    tc_world world;
    tc_world restored = {0};
    tc_player player;
    tc_save_data data;
    float position[3] = {4.5f, 30.0f, 6.5f};
    int height = tc_terrain_height(2, 2, 0x12345678u);
    int ok = 1;

    snprintf(directory, sizeof(directory), "/tmp/thinkcraft-save-%ld",
             (long)getpid());
    snprintf(path, sizeof(path), "%s/nested/world.tc", directory);

    if (!init_world(&arena, &world, 0x12345678u)) {
        return 0;
    }

    tc_player_init(&player, position, 0.75f, -0.2f);

    ok = ok && check(tc_world_set_block(&world, 2, 100, 2, TC_BLOCK_STONE),
                     "stone edit failed")
      && check(tc_world_set_block(&world, 2, 101, 2, TC_BLOCK_SAND),
               "sand edit failed")
      && check(tc_world_set_block(&world, -3, 100, -3, TC_BLOCK_GRASS),
               "negative edit failed")
      && check(tc_world_set_block(&world, 2, height, 2, TC_BLOCK_AIR),
               "surface removal failed")
      && check(tc_world_edit_count(&world) == 4, "wrong edit count")
      && check(tc_world_set_block(&world, 0, 0, 0, TC_BLOCK_AIR) == 0,
               "bedrock edit was accepted")
      && check(tc_world_edit_count(&world) == 4,
               "bedrock attempt changed edit count");

    ok = ok && check(tc_save_ensure_directory(path),
                     "save directory creation failed")
      && check(tc_save_write(path, &world, &player, 2),
               "save write failed")
      && check(tc_save_read(path, &data), "save read failed")
      && check(data.seed == 0x12345678u, "seed round trip failed")
      && check(fabsf(data.position[0] - position[0]) < 0.0001f
               && fabsf(data.position[1] - position[1]) < 0.0001f
               && fabsf(data.position[2] - position[2]) < 0.0001f,
               "position round trip failed")
      && check(fabsf(data.yaw - 0.75f) < 0.0001f
               && fabsf(data.pitch + 0.2f) < 0.0001f,
               "orientation round trip failed")
      && check(data.selected_hotbar == 2, "hotbar round trip failed")
      && check(data.edit_count == 4, "saved edit count mismatch")
      && check(data.edits[0].x == 2 && data.edits[0].y == 100
               && data.edits[0].z == 2
               && data.edits[0].block == TC_BLOCK_STONE,
               "first saved edit mismatch")
      && check(data.edits[2].x == -3 && data.edits[2].y == 100
               && data.edits[2].z == -3
               && data.edits[2].block == TC_BLOCK_GRASS,
               "negative saved edit mismatch");

    if (ok) {
        ok = check(apply_save(&restored_arena, &restored, &data),
                   "save apply failed");
        ok = ok
          && check(tc_world_get_block(&restored, 2, 100, 2)
                   == TC_BLOCK_STONE,
                   "stone edit did not survive regeneration")
          && check(tc_world_get_block(&restored, 2, 101, 2)
                   == TC_BLOCK_SAND,
                   "sand edit did not survive regeneration")
          && check(tc_world_get_block(&restored, -3, 100, -3)
                   == TC_BLOCK_GRASS,
                   "negative edit did not survive regeneration")
          && check(tc_world_get_block(&restored, 2, height, 2)
                   == TC_BLOCK_AIR,
                   "surface removal did not survive regeneration")
          && check(tc_world_update(&restored, 8, 8),
                   "far streaming update failed");
        ok = ok
          && check(tc_world_update(&restored, 0, 0),
                   "return streaming update failed")
          && check(tc_world_get_block(&restored, 2, 100, 2)
                   == TC_BLOCK_STONE,
                   "stone edit was lost after chunk reload")
          && check(tc_world_get_block(&restored, 2, height, 2)
                   == TC_BLOCK_AIR,
                   "surface removal was lost after chunk reload")
          && check(tc_world_edit_count(&restored) == 4,
                   "restored edit count changed");
    }

    if (ok) {
        printf("save round trip: seed %08x, %zu edits, player %.1f %.1f %.1f\n",
               data.seed, data.edit_count, data.position[0],
               data.position[1], data.position[2]);
    }

    tc_save_data_shutdown(&data);
    tc_world_shutdown(&restored);
    tc_arena_shutdown(&restored_arena);
    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    unlink(path);
    {
        char nested[512];
        snprintf(nested, sizeof(nested), "%s/nested", directory);
        rmdir(nested);
        rmdir(directory);
    }

    if (!ok) {
        return 1;
    }

    printf("test_save: all checks passed\n");
    return 0;
}
