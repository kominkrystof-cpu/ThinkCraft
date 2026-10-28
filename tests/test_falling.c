#include "thinkcraft/arena.h"
#include "thinkcraft/chunk_mesh.h"
#include "thinkcraft/falling.h"
#include "thinkcraft/mesh.h"
#include "thinkcraft/world.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_falling: %s\n", message);
    }
    return condition;
}

static int test_single_block(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 5, 0, 5, TC_BLOCK_STONE),
                     "could not place the support block")
      && check(tc_world_set_block(&world, 5, 5, 5, TC_BLOCK_SAND),
               "could not place suspended sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 5, 5, 5) == 1,
               "suspended sand did not start falling")
      && check(tc_falling_count(&falling) == 1,
               "wrong falling block count")
      && check(tc_world_get_block(&world, 5, 5, 5) == TC_BLOCK_AIR,
               "falling sand was not removed from the world");

    ok = ok && check(tc_falling_update(&falling, &world, 1.0f) == 1,
                     "falling sand did not land")
      && check(tc_falling_count(&falling) == 0,
               "landed block stayed in the entity list")
      && check(tc_world_get_block(&world, 5, 1, 5) == TC_BLOCK_SAND,
               "landed sand is not resting on the support")
      && check(tc_world_get_block(&world, 5, 5, 5) == TC_BLOCK_AIR,
               "old sand cell was not cleared");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_column_cascade(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 6, 0, 6, TC_BLOCK_STONE),
                     "could not place the column support");
    for (int y = 1; y <= 5; y++) {
        ok = ok && check(tc_world_set_block(&world, 6, y, 6, TC_BLOCK_SAND),
                         "could not place the sand column");
    }

    ok = ok && check(tc_world_set_block(&world, 6, 0, 6, TC_BLOCK_AIR),
                     "could not remove the column support")
      && check(tc_falling_notify_block_changed(&falling, &world, 6, 0, 6) == 5,
               "sand column did not cascade")
      && check(tc_falling_count(&falling) == 5,
               "wrong cascade entity count");

    ok = ok && check(tc_falling_update(&falling, &world, 1.0f) == 5,
                     "cascading column did not land")
      && check(tc_falling_count(&falling) == 0,
               "cascade entities were not removed");

    for (int y = 0; y < 5; y++) {
        ok = ok && check(tc_world_get_block(&world, 6, y, 6) == TC_BLOCK_SAND,
                         "cascade column did not stack on the floor");
    }

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_supported_and_settle(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);

    ok = ok && check(tc_world_set_block(&world, 7, 0, 7, TC_BLOCK_STONE),
                     "could not place supported sand support")
      && check(tc_world_set_block(&world, 7, 1, 7, TC_BLOCK_SAND),
               "could not place supported sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 7, 1, 7) == 0,
               "supported sand started falling")
      && check(tc_falling_count(&falling) == 0,
               "supported sand created an entity");

    ok = ok && check(tc_world_set_block(&world, 8, 0, 8, TC_BLOCK_STONE),
                     "could not place settle support")
      && check(tc_world_set_block(&world, 8, 6, 8, TC_BLOCK_SAND),
               "could not place high sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 8, 6, 8) == 1,
               "high sand did not fall");

    tc_falling_settle_all(&falling, &world);
    ok = ok && check(tc_falling_count(&falling) == 0,
                     "settle did not clear the entity list")
      && check(tc_world_get_block(&world, 8, 1, 8) == TC_BLOCK_SAND,
               "settled sand is not resting on the support");

    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}


static int slot_dirty(const tc_world *world, int chunk_x, int chunk_z) {
    for (int index = 0; index < world->size * world->size; index++) {
        const tc_world_slot *slot = &world->slots[index];

        if (slot->loaded && slot->chunk_x == chunk_x
            && slot->chunk_z == chunk_z) {
            return slot->dirty;
        }
    }

    return 0;
}

static int quad_normal(const tc_mesh *mesh, size_t quad, int normal[3]) {
    const tc_vertex *v = &mesh->vertices[quad * 4];
    int ax = (int)v[1].position[0] - (int)v[0].position[0];
    int ay = (int)v[1].position[1] - (int)v[0].position[1];
    int az = (int)v[1].position[2] - (int)v[0].position[2];
    int bx = (int)v[2].position[0] - (int)v[0].position[0];
    int by = (int)v[2].position[1] - (int)v[0].position[1];
    int bz = (int)v[2].position[2] - (int)v[0].position[2];
    int nx = ay * bz - az * by;
    int ny = az * bx - ax * bz;
    int nz = ax * by - ay * bx;
    int magnitude = abs(nx) + abs(ny) + abs(nz);

    if (magnitude == 0) {
        return 0;
    }

    normal[0] = nx / magnitude;
    normal[1] = ny / magnitude;
    normal[2] = nz / magnitude;
    return 1;
}

static int mesh_has_face(const tc_mesh *mesh, int nx, int ny, int nz,
                         int min_x, int min_y, int min_z,
                         int max_x, int max_y, int max_z) {
    for (size_t quad = 0; quad < mesh->quad_count; quad++) {
        int normal[3];
        int inside = 1;

        if (!quad_normal(mesh, quad, normal)
            || normal[0] != nx || normal[1] != ny || normal[2] != nz) {
            continue;
        }

        for (size_t vertex = quad * 4; vertex < quad * 4 + 4; vertex++) {
            const tc_vertex *v = &mesh->vertices[vertex];
            int x = (int)v->position[0] + (int)mesh->origin[0];
            int y = (int)v->position[1] + (int)mesh->origin[1];
            int z = (int)v->position[2] + (int)mesh->origin[2];

            if (x < min_x || x > max_x || y < min_y || y > max_y
                || z < min_z || z > max_z) {
                inside = 0;
                break;
            }
        }

        if (inside) {
            return 1;
        }
    }

    return 0;
}

static int test_border_remesh(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    tc_mesh mesh;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);
    tc_mesh_init(&mesh);

    ok = ok && check(tc_world_set_block(&world, 15, 10, 5, TC_BLOCK_SAND),
                     "could not place lower border sand")
      && check(tc_world_set_block(&world, 15, 11, 5, TC_BLOCK_SAND),
               "could not place upper border sand")
      && check(tc_world_set_block(&world, 16, 11, 5, TC_BLOCK_SAND),
               "could not place adjacent border sand");

    for (int index = 0; index < world.size * world.size; index++) {
        world.slots[index].dirty = 0;
    }

    tc_world_mark_dirty(&world, 15, 11, 5);
    ok = ok && check(slot_dirty(&world, 0, 0),
                     "single dirty mark missed its chunk")
      && check(!slot_dirty(&world, 1, 0),
               "single dirty mark touched a neighbor");

    for (int index = 0; index < world.size * world.size; index++) {
        world.slots[index].dirty = 0;
    }

    tc_world_mark_dirty_neighbors(&world, 15, 11, 5);
    ok = ok && check(slot_dirty(&world, 0, 0),
                     "neighbor dirty mark missed its chunk")
      && check(slot_dirty(&world, 1, 0),
               "neighbor dirty mark missed the border neighbor");

    for (int index = 0; index < world.size * world.size; index++) {
        world.slots[index].dirty = 0;
    }

    ok = ok && check(tc_world_set_block(&world, 15, 11, 5, TC_BLOCK_AIR),
                     "could not remove upper border sand")
      && check(tc_falling_notify_block_changed(&falling, &world, 15, 11, 5)
               == 0, "removed sand spawned a falling block")
      && check(slot_dirty(&world, 0, 0), "edited chunk was not marked dirty")
      && check(slot_dirty(&world, 1, 0),
               "border neighbor was not marked dirty");

    ok = ok && check(tc_world_build_chunk_mesh(&world, 0, 0, &mesh, NULL),
                     "could not rebuild edited chunk mesh")
      && check(mesh_has_face(&mesh, 0, 1, 0, 15, 11, 5, 16, 11, 6),
               "lower sand lost its new top face");

    ok = ok && check(tc_world_build_chunk_mesh(&world, 1, 0, &mesh, NULL),
                     "could not rebuild neighbor chunk mesh")
      && check(mesh_has_face(&mesh, -1, 0, 0, 16, 11, 5, 16, 12, 6),
               "neighbor sand lost its new side face");

    tc_mesh_shutdown(&mesh);
    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

static int test_border_cascade(void) {
    tc_arena arena;
    tc_world world;
    tc_falling_blocks falling;
    tc_mesh mesh;
    int ok = 1;

    if (!tc_arena_init(&arena, 2 * 1024 * 1024)
        || !tc_world_init(&world, &arena, 1)) {
        return 0;
    }

    tc_falling_init(&falling);
    tc_mesh_init(&mesh);

    ok = ok && check(tc_world_set_block(&world, 15, 9, 5, TC_BLOCK_STONE),
                     "could not place border support")
      && check(tc_world_set_block(&world, 15, 10, 5, TC_BLOCK_SAND),
               "could not place border sand")
      && check(tc_world_set_block(&world, 15, 11, 5, TC_BLOCK_SAND),
               "could not place upper border sand")
      && check(tc_world_set_block(&world, 16, 11, 5, TC_BLOCK_SAND),
               "could not place adjacent border sand");

    for (int index = 0; index < world.size * world.size; index++) {
        world.slots[index].dirty = 0;
    }

    ok = ok && check(tc_world_set_block(&world, 15, 9, 5, TC_BLOCK_AIR),
                     "could not remove border support")
      && check(tc_falling_notify_block_changed(&falling, &world, 15, 9, 5)
               == 2, "border sand column did not cascade")
      && check(slot_dirty(&world, 0, 0), "cascade chunk was not marked dirty")
      && check(slot_dirty(&world, 1, 0),
               "cascade border neighbor was not marked dirty");

    ok = ok && check(tc_world_build_chunk_mesh(&world, 1, 0, &mesh, NULL),
                     "could not rebuild cascade neighbor mesh")
      && check(mesh_has_face(&mesh, -1, 0, 0, 16, 11, 5, 16, 12, 6),
               "neighbor sand lost its side face after cascade");

    tc_mesh_shutdown(&mesh);
    tc_world_shutdown(&world);
    tc_arena_shutdown(&arena);
    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_single_block();
    ok = ok && test_column_cascade();
    ok = ok && test_supported_and_settle();
    ok = ok && test_border_remesh();
    ok = ok && test_border_cascade();

    if (!ok) {
        return 1;
    }

    printf("test_falling: all checks passed\n");
    return 0;
}
