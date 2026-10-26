#include "thinkcraft/arena.h"
#include "thinkcraft/chunk.h"
#include "thinkcraft/chunk_mesh.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/terrain.h"
#include "thinkcraft/texture.h"
#include "thinkcraft/world.h"

#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_world: %s\n", message);
    }
    return condition;
}

static int validate_mesh(const tc_mesh *mesh, const tc_chunk *chunk,
                         size_t expected_quads) {
    if (!check(mesh->quad_count == expected_quads, "wrong quad count")
        || !check(mesh->vertex_count == expected_quads * 4,
                  "wrong vertex count")
        || !check(mesh->index_count == expected_quads * 6,
                  "wrong index count")) {
        return 0;
    }

    for (size_t i = 0; i < mesh->index_count; i++) {
        if (!check(mesh->indices[i] < mesh->vertex_count,
                   "index out of range")) {
            return 0;
        }
    }

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        const tc_vertex *vertex = &mesh->vertices[i];
        float min_x = (float)chunk->origin_x;
        float min_y = (float)chunk->origin_y;
        float min_z = (float)chunk->origin_z;
        float max_x = min_x + (float)TC_CHUNK_SIZE_X;
        float max_y = min_y + (float)TC_CHUNK_SIZE_Y;
        float max_z = min_z + (float)TC_CHUNK_SIZE_Z;

        if (!check(vertex->position[0] >= min_x
                   && vertex->position[0] <= max_x,
                   "x position out of chunk range")
            || !check(vertex->position[1] >= min_y
                      && vertex->position[1] <= max_y,
                      "y position out of chunk range")
            || !check(vertex->position[2] >= min_z
                      && vertex->position[2] <= max_z,
                      "z position out of chunk range")
            || !check(vertex->tile < TC_TILE_COUNT, "invalid vertex tile")) {
            return 0;
        }
    }

    return 1;
}

static size_t loaded_slots(const tc_world *world) {
    size_t count = 0;

    for (int index = 0; index < world->size * world->size; index++) {
        if (world->slots[index].loaded) {
            count++;
        }
    }

    return count;
}

static int test_world_access(void) {
    tc_arena arena;
    tc_world world;
    tc_chunk *chunk;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    chunk = tc_world_chunk_at(&world, 1, 1);
    ok = ok && check(world.radius == 1 && world.size == 3,
                     "wrong world dimensions")
      && check(loaded_slots(&world) == 9, "wrong loaded chunk count")
      && check(chunk != NULL, "missing chunk")
      && check(chunk->origin_x == 16 && chunk->origin_y == 0
               && chunk->origin_z == 16, "wrong chunk origin")
      && check(tc_world_chunk_at(&world, -1, -1) != NULL,
               "negative chunk index rejected")
      && check(tc_world_chunk_at(&world, -2, 0) == NULL,
               "out-of-range negative chunk index accepted")
      && check(tc_world_chunk_at(&world, 2, 0) == NULL,
               "out-of-range chunk index accepted")
      && check(tc_world_set_block(&world, 17, 4, 18, TC_BLOCK_STONE),
               "world set failed")
      && check(tc_world_get_block(&world, 17, 4, 18) == TC_BLOCK_STONE,
               "world get failed")
      && check(chunk->solid_count == 1, "chunk solid count not updated")
      && check(tc_world_set_block(&world, -1, 4, -1, TC_BLOCK_STONE),
               "negative world set failed")
      && check(tc_world_get_block(&world, -1, 4, -1) == TC_BLOCK_STONE,
               "negative world get failed")
      && check(tc_world_chunk_at(&world, -1, -1)->solid_count == 1,
               "negative chunk solid count not updated")
      && check(tc_world_set_block(&world, 17, 4, 18, TC_BLOCK_AIR),
               "world clear failed")
      && check(chunk->solid_count == 0, "chunk solid count not cleared")
      && check(tc_world_set_block(&world, 48, 4, 0, TC_BLOCK_STONE) == 0,
               "out-of-range world set accepted")
      && check(tc_world_get_block(&world, 48, 4, 0) == TC_BLOCK_AIR,
               "out-of-range world get was not air")
      && check(tc_world_get_block(&world, -32, 4, 0) == TC_BLOCK_AIR,
               "far negative world get was not air")
      && check(tc_world_get_block(&world, 0, 256, 0) == TC_BLOCK_AIR,
               "high world get was not air");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_streaming_update(void) {
    tc_arena arena;
    tc_world world;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    ok = ok && check(tc_world_update(&world, 2, 0), "world update failed")
      && check(world.center_chunk_x == 2 && world.center_chunk_z == 0,
               "world center not updated")
      && check(tc_world_chunk_at(&world, -1, 0) == NULL,
               "unloaded chunk still accessible")
      && check(tc_world_chunk_at(&world, 0, 0) == NULL,
               "old edge chunk still accessible")
      && check(tc_world_chunk_at(&world, 1, 0) != NULL,
               "retained chunk missing")
      && check(tc_world_chunk_at(&world, 3, 0) != NULL,
               "new chunk missing")
      && check(tc_world_chunk_at(&world, 3, 0)->origin_x == 48,
               "new chunk origin wrong")
      && check(loaded_slots(&world) == 9, "wrong streamed chunk count")
      && check(tc_world_get_block(&world, -1, 4, 0) == TC_BLOCK_AIR,
               "unloaded block was accessible")
      && check(tc_world_update(&world, -2, -2),
               "negative world update failed")
      && check(tc_world_chunk_at(&world, -3, -3) != NULL,
               "negative edge chunk missing")
      && check(tc_world_chunk_at(&world, -3, -3)->origin_x == -48
               && tc_world_chunk_at(&world, -3, -3)->origin_z == -48,
               "negative chunk origin wrong")
      && check(loaded_slots(&world) == 9,
               "wrong negative streamed chunk count");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_boundary_mesh(void) {
    tc_arena arena;
    tc_world world;
    tc_mesh mesh;
    tc_mesh_stats stats_left;
    tc_mesh_stats stats_right;
    size_t total_quads;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    for (int y = 4; y <= 7; y++) {
        for (int z = 4; z <= 7; z++) {
            for (int x = 14; x <= 17; x++) {
                ok = ok && check(tc_world_set_block(&world, x, y, z,
                                                    TC_BLOCK_STONE),
                                 "boundary fill failed");
            }
        }
    }

    tc_mesh_init(&mesh);

    ok = ok && check(tc_world_build_chunk_mesh(&world, 0, 0, &mesh,
                                               &stats_left),
                     "left chunk mesh build failed");
    ok = ok && check(stats_left.solid_blocks == 32,
                     "left solid block count mismatch");
    ok = ok && check(stats_left.visible_faces == 48,
                     "left visible face count mismatch");
    ok = ok && check(stats_left.quads == 5, "left quad count mismatch");
    ok = ok && validate_mesh(&mesh, tc_world_chunk_at(&world, 0, 0), 5);

    ok = ok && check(tc_world_build_chunk_mesh(&world, 1, 0, &mesh,
                                               &stats_right),
                     "right chunk mesh build failed");
    ok = ok && check(stats_right.solid_blocks == 32,
                     "right solid block count mismatch");
    ok = ok && check(stats_right.visible_faces == 48,
                     "right visible face count mismatch");
    ok = ok && check(stats_right.quads == 5, "right quad count mismatch");
    ok = ok && validate_mesh(&mesh, tc_world_chunk_at(&world, 1, 0), 5);

    total_quads = stats_left.quads + stats_right.quads;
    ok = ok && check(total_quads == 10,
                     "chunk boundary faces were not culled");

    printf("boundary mesh: %zu quads, %zu visible faces across two chunks\n",
           total_quads, stats_left.visible_faces + stats_right.visible_faces);

    tc_mesh_shutdown(&mesh);
    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_generated_world(void) {
    tc_arena arena;
    tc_world world;
    tc_mesh mesh;
    tc_mesh_stats stats;
    const tc_chunk *center;
    uint8_t saved_block;
    size_t total_solid = 0;
    int ok = 1;

    if (!tc_arena_init(&arena, 4 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    tc_world_generate_terrain(&world);

    for (int index = 0; index < world.size * world.size; index++) {
        const tc_chunk *chunk = &world.slots[index].chunk;
        uint32_t counted;

        if (!world.slots[index].loaded) {
            continue;
        }

        counted = tc_chunk_count_solid(chunk->blocks, TC_CHUNK_BLOCK_COUNT);
        ok = ok && check(chunk->solid_count > 0, "empty generated chunk");
        ok = ok && check(chunk->solid_count == counted,
                         "generated solid count mismatch");
        total_solid += chunk->solid_count;
    }

    ok = ok && check(total_solid > 0, "generated world is empty")
      && check(tc_world_get_block(&world, 0, 0, 0) != TC_BLOCK_AIR,
               "generated ground is missing")
      && check(tc_world_get_block(&world, 0, 255, 0) == TC_BLOCK_AIR,
               "generated world exceeds height limit");

    center = tc_world_chunk_at(&world, 0, 0);
    tc_mesh_init(&mesh);
    ok = ok && check(tc_world_build_chunk_mesh(&world, 0, 0, &mesh, &stats),
                     "generated mesh build failed");
    ok = ok && check(stats.solid_blocks == center->solid_count,
                     "generated chunk solid count mismatch")
      && check(stats.quads > 0, "generated chunk mesh is empty")
      && check(stats.visible_faces > stats.quads,
               "generated chunk was not greedily merged")
      && validate_mesh(&mesh, center, stats.quads);

    saved_block = tc_world_get_block(&world, 8, 5, 8);
    ok = ok && check(saved_block != TC_BLOCK_AIR,
                     "saved generated block is air")
      && check(tc_world_update(&world, 20, 20), "far generation update failed")
      && check(tc_world_get_block(&world, 8, 5, 8) == TC_BLOCK_AIR,
               "far chunk was not unloaded")
      && check(tc_world_get_block(&world, 320, 5, 320) != TC_BLOCK_AIR,
               "far generated chunk is empty")
      && check(tc_world_update(&world, 0, 0), "return update failed")
      && check(tc_world_get_block(&world, 8, 5, 8) == saved_block,
               "generated terrain was not deterministic");

    printf("generated world: 3x3 chunks, %zu solid blocks, "
           "center chunk %zu faces -> %zu quads\n",
           total_solid, stats.visible_faces, stats.quads);

    tc_mesh_shutdown(&mesh);
    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}


static uint32_t digest_chunk(const tc_chunk *chunk) {
    uint32_t digest = 2166136261u;

    for (size_t index = 0; index < TC_CHUNK_BLOCK_COUNT; index++) {
        digest ^= chunk->blocks[index];
        digest *= 16777619u;
    }

    return digest;
}

static int test_edit_persistence(void) {
    tc_arena arena;
    tc_world world;
    const uint32_t seed = 0x0badc0deu;
    int height;
    int ok = 1;

    if (!tc_arena_init(&arena, 4 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    tc_world_set_seed(&world, seed);
    tc_world_generate_terrain(&world);
    height = tc_terrain_height(2, 2, seed);

    ok = ok && check(tc_world_set_block(&world, 2, 100, 2,
                                        TC_BLOCK_STONE),
                     "stone edit failed")
      && check(tc_world_set_block(&world, 2, height, 2,
                                  TC_BLOCK_AIR),
               "surface removal failed")
      && check(tc_world_set_block(&world, 20, 100, 2, TC_BLOCK_SAND),
               "positive chunk edit failed")
      && check(tc_world_set_block(&world, 35, 100, 35,
                                  TC_BLOCK_STONE) == 0,
               "unloaded world edit accepted")
      && check(tc_world_edit_count(&world) == 3, "wrong persistent edit count")
      && check(tc_world_update(&world, 2, 2), "far edit update failed")
      && check(tc_world_update(&world, 0, 0), "return edit update failed")
      && check(tc_world_get_block(&world, 2, 100, 2) == TC_BLOCK_STONE,
               "stone edit was lost after reload")
      && check(tc_world_get_block(&world, 2, height, 2) == TC_BLOCK_AIR,
               "surface removal was lost after reload")
      && check(tc_world_get_block(&world, 20, 100, 2) == TC_BLOCK_SAND,
               "positive chunk edit was lost after reload")
      && check(tc_world_edit_count(&world) == 3,
               "persistent edit count changed after reload");

    tc_world_set_seed(&world, seed);
    tc_world_generate_terrain(&world);
    ok = ok && check(tc_world_edit_count(&world) == 0,
                     "seed change did not clear edits")
      && check(tc_world_get_block(&world, 2, 100, 2) == TC_BLOCK_AIR,
               "cleared edit still present after regeneration");

    printf("edit persistence: %zu edits replayed across chunk reloads\n",
           tc_world_edit_count(&world));

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_seeded_terrain(void) {
    tc_arena arena;
    tc_world world;
    const tc_chunk *center;
    uint32_t first_digest;
    uint32_t second_digest;
    uint32_t repeat_digest;
    size_t cave_blocks = 0;
    size_t surface_holes = 0;
    size_t bedrock_errors = 0;
    int ok = 1;

    if (!tc_arena_init(&arena, 4 * 1024 * 1024)) {
        return 0;
    }

    if (!tc_world_init(&world, &arena, 1)) {
        tc_arena_shutdown(&arena);
        return 0;
    }

    tc_world_set_seed(&world, 0x13579bdfu);
    tc_world_generate_terrain(&world);

    center = tc_world_chunk_at(&world, 0, 0);
    ok = ok && check(center != NULL, "seeded center chunk missing");
    first_digest = digest_chunk(center);

    for (int index = 0; index < world.size * world.size; index++) {
        const tc_world_slot *slot = &world.slots[index];

        if (!slot->loaded) {
            continue;
        }

        for (int z = 0; z < TC_CHUNK_SIZE_Z; z++) {
            for (int x = 0; x < TC_CHUNK_SIZE_X; x++) {
                int world_x = slot->chunk.origin_x + x;
                int world_z = slot->chunk.origin_z + z;
                int height = tc_terrain_height(world_x, world_z, world.seed);

                if (!tc_block_is_solid(
                        tc_world_get_block(&world, world_x, height, world_z))
                    || tc_world_get_block(&world, world_x, height + 1,
                                          world_z) != TC_BLOCK_AIR) {
                    surface_holes++;
                }

                for (int y = 0; y < TC_WORLD_BEDROCK_LAYERS; y++) {
                    if (tc_world_get_block(&world, world_x, y, world_z)
                        != TC_BLOCK_BEDROCK) {
                        bedrock_errors++;
                    }
                }

                for (int y = 3; y <= height - 4; y++) {
                    if (tc_world_get_block(&world, world_x, y, world_z)
                        == TC_BLOCK_AIR) {
                        cave_blocks++;
                    }
                }
            }
        }
    }

    ok = ok && check(surface_holes == 0, "cave breached the surface")
      && check(cave_blocks > 0, "seeded terrain has no caves")
      && check(bedrock_errors == 0, "generated floor is not bedrock")
      && check(tc_world_set_block(&world, 0, 0, 0, TC_BLOCK_AIR) == 0,
               "bedrock was breakable")
      && check(tc_world_get_block(&world, 0, 0, 0) == TC_BLOCK_BEDROCK,
               "bedrock was removed")
      && check(tc_world_set_block(&world, 0, 0, 0, TC_BLOCK_DIRT) == 0,
               "bedrock was replaceable")
      && check(tc_world_get_block(&world, 0, 0, 0) == TC_BLOCK_BEDROCK,
               "bedrock was replaced");

    tc_world_set_seed(&world, 0x2468ace0u);
    tc_world_generate_terrain(&world);
    second_digest = digest_chunk(tc_world_chunk_at(&world, 0, 0));
    ok = ok && check(first_digest != second_digest,
                     "different seeds produced identical terrain");

    tc_world_set_seed(&world, 0x13579bdfu);
    tc_world_generate_terrain(&world);
    repeat_digest = digest_chunk(tc_world_chunk_at(&world, 0, 0));
    ok = ok && check(first_digest == repeat_digest,
                     "same seed was not deterministic");

    printf("seeded terrain: digest %08x, %zu cave blocks, "
           "%zu surface holes, %zu bedrock errors\n",
           first_digest, cave_blocks, surface_holes, bedrock_errors);

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_world_access();
    ok = ok && test_streaming_update();
    ok = ok && test_boundary_mesh();
    ok = ok && test_generated_world();
    ok = ok && test_edit_persistence();
    ok = ok && test_seeded_terrain();

    if (!ok) {
        return 1;
    }

    printf("test_world: all checks passed\n");
    return 0;
}
