#include "thinkcraft/entity.h"
#include "thinkcraft/log.h"
#include "thinkcraft/math.h"
#include "thinkcraft/mesh.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define TC_MODEL_UNIT (1.0f / 64.0f)
#define TC_PI 3.14159265358979323846f

static void set_vertex(tc_vertex *vertex, int x, int y, int z, int u, int v,
                       int shade, int tile) {
    vertex->position[0] = (uint16_t)x;
    vertex->position[1] = (uint16_t)y;
    vertex->position[2] = (uint16_t)z;
    vertex->uv[0] = (uint8_t)u;
    vertex->uv[1] = (uint8_t)v;
    vertex->shade = (uint8_t)shade;
    vertex->tile = (uint8_t)tile;
}

static int add_face(tc_mesh *mesh, int x0, int y0, int z0,
                    int x1, int y1, int z1, int face, int shade,
                    int tile) {
    tc_vertex quad[4];

    switch (face) {
        case 0:
            set_vertex(&quad[0], x0, y1, z1, 0, 0, shade, tile);
            set_vertex(&quad[1], x1, y1, z1, 1, 0, shade, tile);
            set_vertex(&quad[2], x1, y1, z0, 1, 1, shade, tile);
            set_vertex(&quad[3], x0, y1, z0, 0, 1, shade, tile);
            break;
        case 1:
            set_vertex(&quad[0], x0, y0, z0, 0, 1, shade, tile);
            set_vertex(&quad[1], x1, y0, z0, 1, 1, shade, tile);
            set_vertex(&quad[2], x1, y0, z1, 1, 0, shade, tile);
            set_vertex(&quad[3], x0, y0, z1, 0, 0, shade, tile);
            break;
        case 2:
            set_vertex(&quad[0], x1, y0, z1, 0, 0, shade, tile);
            set_vertex(&quad[1], x1, y0, z0, 1, 0, shade, tile);
            set_vertex(&quad[2], x1, y1, z0, 1, 1, shade, tile);
            set_vertex(&quad[3], x1, y1, z1, 0, 1, shade, tile);
            break;
        case 3:
            set_vertex(&quad[0], x0, y0, z0, 0, 0, shade, tile);
            set_vertex(&quad[1], x0, y0, z1, 1, 0, shade, tile);
            set_vertex(&quad[2], x0, y1, z1, 1, 1, shade, tile);
            set_vertex(&quad[3], x0, y1, z0, 0, 1, shade, tile);
            break;
        case 4:
            set_vertex(&quad[0], x0, y0, z1, 0, 0, shade, tile);
            set_vertex(&quad[1], x1, y0, z1, 1, 0, shade, tile);
            set_vertex(&quad[2], x1, y1, z1, 1, 1, shade, tile);
            set_vertex(&quad[3], x0, y1, z1, 0, 1, shade, tile);
            break;
        case 5:
        default:
            set_vertex(&quad[0], x1, y0, z0, 0, 0, shade, tile);
            set_vertex(&quad[1], x0, y0, z0, 1, 0, shade, tile);
            set_vertex(&quad[2], x0, y1, z0, 1, 1, shade, tile);
            set_vertex(&quad[3], x1, y1, z0, 0, 1, shade, tile);
            break;
    }

    return tc_mesh_add_quad(mesh, quad);
}

static int add_box(tc_mesh *mesh, int x0, int y0, int z0,
                   int width, int height, int depth, int top_tile,
                   int bottom_tile, int east_tile, int west_tile,
                   int south_tile, int north_tile) {
    int x1 = x0 + width;
    int y1 = y0 + height;
    int z1 = z0 + depth;

    return add_face(mesh, x0, y0, z0, x1, y1, z1, 0, 255, top_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 1, 140, bottom_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 2, 212, east_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 3, 190, west_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 4, 202, south_tile)
        && add_face(mesh, x0, y0, z0, x1, y1, z1, 5, 174, north_tile);
}

static int add_uniform_box(tc_mesh *mesh, int x0, int y0, int z0,
                           int width, int height, int depth, int tile) {
    return add_box(mesh, x0, y0, z0, width, height, depth, tile, tile, tile,
                   tile, tile, tile);
}

static int add_tool_plane(tc_mesh *mesh, int tile) {
    tc_vertex quad[4];

    set_vertex(&quad[0], 0, 0, 0, 0, 0, 255, tile);
    set_vertex(&quad[1], 64, 0, 0, 1, 0, 255, tile);
    set_vertex(&quad[2], 64, 64, 0, 1, 1, 255, tile);
    set_vertex(&quad[3], 0, 64, 0, 0, 1, 255, tile);

    return tc_mesh_add_quad(mesh, quad);
}

static int upload_mesh(tc_renderer *renderer, tc_render_mesh *target,
                       tc_mesh *mesh) {
    int result = tc_renderer_upload_mesh(renderer, target, mesh);

    tc_mesh_shutdown(mesh);
    return result;
}

static int build_block_mesh(tc_renderer *renderer, tc_render_mesh *target,
                            uint8_t block) {
    tc_mesh mesh;
    int top;
    int side;
    int bottom;

    tc_mesh_init(&mesh);

    switch (block) {
        case TC_BLOCK_GRASS:
            top = TC_TILE_GRASS_TOP;
            side = TC_TILE_GRASS_SIDE;
            bottom = TC_TILE_DIRT;
            break;
        case TC_BLOCK_DIRT:
            top = side = bottom = TC_TILE_DIRT;
            break;
        case TC_BLOCK_STONE:
            top = side = bottom = TC_TILE_STONE;
            break;
        case TC_BLOCK_SAND:
            top = side = bottom = TC_TILE_SAND;
            break;
        case TC_BLOCK_BEDROCK:
            top = side = bottom = TC_TILE_BEDROCK;
            break;
        default:
            tc_mesh_shutdown(&mesh);
            return 1;
    }

    if (!add_box(&mesh, 0, 0, 0, 64, 64, 64, top, bottom, side, side, side,
                 side)) {
        tc_mesh_shutdown(&mesh);
        return 0;
    }

    return upload_mesh(renderer, target, &mesh);
}

static int build_tool_mesh(tc_renderer *renderer, tc_render_mesh *target,
                           int tile) {
    tc_mesh mesh;

    tc_mesh_init(&mesh);
    if (!add_tool_plane(&mesh, tile)) {
        tc_mesh_shutdown(&mesh);
        return 0;
    }

    return upload_mesh(renderer, target, &mesh);
}

static int build_player_meshes(tc_entity_renderer *entities,
                               tc_renderer *renderer) {
    tc_mesh mesh;

    tc_mesh_init(&mesh);
    if (!add_box(&mesh, 0, 0, 0, 28, 28, 28, TC_TILE_HAIR, TC_TILE_SKIN,
                 TC_TILE_HAIR, TC_TILE_HAIR, TC_TILE_HAIR, TC_TILE_FACE)
        || !upload_mesh(renderer, &entities->head, &mesh)) {
        return 0;
    }

    tc_mesh_init(&mesh);
    if (!add_uniform_box(&mesh, 0, 0, 0, 24, 44, 14, TC_TILE_SHIRT)
        || !upload_mesh(renderer, &entities->torso, &mesh)) {
        return 0;
    }

    tc_mesh_init(&mesh);
    if (!add_uniform_box(&mesh, 0, 0, 0, 12, 28, 12, TC_TILE_SHIRT)
        || !add_uniform_box(&mesh, 0, 28, 0, 12, 16, 12, TC_TILE_SKIN)
        || !upload_mesh(renderer, &entities->arm, &mesh)) {
        return 0;
    }

    tc_mesh_init(&mesh);
    if (!add_uniform_box(&mesh, 0, 0, 0, 12, 44, 12, TC_TILE_PANTS)
        || !upload_mesh(renderer, &entities->leg, &mesh)) {
        return 0;
    }

    tc_mesh_init(&mesh);
    if (!add_uniform_box(&mesh, 0, 0, 0, 10, 34, 10, TC_TILE_SKIN)
        || !upload_mesh(renderer, &entities->hand_arm, &mesh)) {
        return 0;
    }

    return 1;
}

int tc_entity_renderer_init(tc_entity_renderer *entities,
                            tc_renderer *renderer) {
    if (entities == NULL || renderer == NULL) {
        return 0;
    }

    memset(entities, 0, sizeof(*entities));

    for (int block = 0; block < 6; block++) {
        if (!build_block_mesh(renderer, &entities->blocks[block],
                              (uint8_t)block)) {
            tc_entity_renderer_shutdown(entities, renderer);
            return 0;
        }
    }

    for (int item = 0; item < TC_ITEM_COUNT; item++) {
        const tc_item_def *definition = tc_item_get((uint16_t)item);

        if (definition != NULL && definition->block == TC_BLOCK_AIR
            && !build_tool_mesh(renderer, &entities->tools[item],
                                definition->icon_tile)) {
            tc_entity_renderer_shutdown(entities, renderer);
            return 0;
        }
    }

    if (!build_player_meshes(entities, renderer)) {
        tc_log_error("failed to build player meshes");
        tc_entity_renderer_shutdown(entities, renderer);
        return 0;
    }

    return 1;
}

void tc_entity_renderer_shutdown(tc_entity_renderer *entities,
                                 tc_renderer *renderer) {
    (void)renderer;

    if (entities == NULL) {
        return;
    }

    for (int block = 0; block < 6; block++) {
        tc_renderer_destroy_mesh(&entities->blocks[block]);
    }
    for (int item = 0; item < TC_ITEM_COUNT; item++) {
        tc_renderer_destroy_mesh(&entities->tools[item]);
    }
    tc_renderer_destroy_mesh(&entities->head);
    tc_renderer_destroy_mesh(&entities->torso);
    tc_renderer_destroy_mesh(&entities->arm);
    tc_renderer_destroy_mesh(&entities->leg);
    tc_renderer_destroy_mesh(&entities->hand_arm);
    memset(entities, 0, sizeof(*entities));
}

static void part_matrix(float out[16], const float base[16],
                        float x, float y, float z, float rx, int flip_y,
                        float scale, float center_x, float center_y,
                        float center_z) {
    float translate[16];
    float rotate[16];
    float scale_matrix[16];
    float center[16];
    float local[16];
    float temporary[16];

    tc_mat4_translate(translate, x, y, z);
    tc_mat4_rotate_x(rotate, rx);
    tc_mat4_scale(scale_matrix, scale, flip_y ? -scale : scale, scale);
    tc_mat4_translate(center, -center_x, -center_y, -center_z);
    tc_mat4_mul(local, translate, rotate);
    tc_mat4_mul(temporary, local, scale_matrix);
    tc_mat4_mul(local, temporary, center);
    tc_mat4_mul(out, base, local);
}

static void attachment_matrix(float out[16], const float parent[16],
                              float scale, int flip_y, float center_x,
                              float center_y, float center_z) {
    float scale_matrix[16];
    float center[16];
    float local[16];

    tc_mat4_scale(scale_matrix, scale, flip_y ? -scale : scale, scale);
    tc_mat4_translate(center, -center_x, -center_y, -center_z);
    tc_mat4_mul(local, scale_matrix, center);
    tc_mat4_mul(out, parent, local);
}

static void item_matrix(float out[16], const float parent[16],
                        float x, float y, float z, float yaw, float pitch,
                        float scale, uint16_t item) {
    float translate[16];
    float rotate_y[16];
    float rotate_x[16];
    float scale_matrix[16];
    float center[16];
    float local[16];
    float temporary[16];
    int block = tc_item_block(item);

    tc_mat4_translate(translate, x, y, z);
    tc_mat4_rotate_y(rotate_y, yaw);
    tc_mat4_rotate_x(rotate_x, pitch);
    tc_mat4_scale(scale_matrix, scale, scale, scale);
    if (block != TC_BLOCK_AIR) {
        tc_mat4_translate(center, -32.0f, -32.0f, -32.0f);
    } else {
        tc_mat4_translate(center, -32.0f, -32.0f, 0.0f);
    }
    tc_mat4_mul(local, translate, rotate_y);
    tc_mat4_mul(temporary, local, rotate_x);
    tc_mat4_mul(local, temporary, scale_matrix);
    tc_mat4_mul(temporary, local, center);
    tc_mat4_mul(out, parent, temporary);
}

static void draw_item(const tc_entity_renderer *entities,
                      tc_renderer *renderer, uint16_t item,
                      const float parent[16], float x, float y, float z,
                      float yaw, float pitch, float scale) {
    const tc_item_def *definition = tc_item_get(item);
    uint8_t block = tc_item_block(item);
    float model[16];
    const tc_render_mesh *mesh = NULL;

    if (definition == NULL) {
        return;
    }

    if (block != TC_BLOCK_AIR) {
        mesh = &entities->blocks[block];
    } else {
        mesh = &entities->tools[item];
    }

    if (mesh->index_count == 0) {
        return;
    }

    item_matrix(model, parent, x, y, z, yaw, pitch, scale, item);
    tc_renderer_draw_model(renderer, mesh, model);
}

void tc_entity_draw_first_person(const tc_entity_renderer *entities,
                                 tc_renderer *renderer, const float eye[3],
                                 float yaw, float pitch, uint16_t item,
                                 float swing, float walk_amount,
                                 float walk_phase) {
    float view_translate[16];
    float view_yaw[16];
    float view_pitch[16];
    float view[16];
    float temporary[16];
    float arm_translate[16];
    float arm_rotate[16];
    float arm_parent[16];
    float arm_model[16];
    float swing_curve = sinf(swing * TC_PI);
    float bob = sinf(walk_phase) * 0.010f * walk_amount;
    float sway = sinf(walk_phase * 0.5f) * 0.008f * walk_amount;
    float hand_x = 0.30f + sway - swing_curve * 0.05f;
    float hand_y = -0.24f + bob - swing_curve * 0.06f;
    float hand_z = -0.55f + swing_curve * 0.04f;
    float arm_pitch = -0.90f - swing_curve * 0.55f;

    tc_mat4_translate(view_translate, eye[0], eye[1], eye[2]);
    tc_mat4_rotate_y(view_yaw, -yaw);
    tc_mat4_rotate_x(view_pitch, pitch);
    tc_mat4_mul(temporary, view_translate, view_yaw);
    tc_mat4_mul(view, temporary, view_pitch);

    tc_mat4_translate(arm_translate, hand_x, hand_y, hand_z);
    tc_mat4_rotate_x(arm_rotate, arm_pitch);
    tc_mat4_mul(temporary, view, arm_translate);
    tc_mat4_mul(arm_parent, temporary, arm_rotate);

    attachment_matrix(arm_model, arm_parent, 0.0068f, 1, 5.0f, 0.0f, 5.0f);
    tc_renderer_draw_model(renderer, &entities->hand_arm, arm_model);

    if (item == TC_ITEM_NONE || tc_item_get(item) == NULL) {
        return;
    }

    if (tc_item_block(item) != TC_BLOCK_AIR) {
        draw_item(entities, renderer, item, arm_parent,
                  -0.09f, -0.04f, -0.02f,
                  0.65f, 0.55f,
                  0.18f * TC_MODEL_UNIT);
    } else {
        draw_item(entities, renderer, item, arm_parent,
                  -0.02f, -0.02f, -0.02f,
                  0.18f - swing_curve * 0.10f,
                  0.80f + swing_curve * 0.35f,
                  0.0052f);
    }
}

void tc_entity_draw_player(const tc_entity_renderer *entities,
                           tc_renderer *renderer, const tc_player *player,
                           uint16_t item, float swing, float walk_amount,
                           float walk_phase) {
    float base_translate[16];
    float base_yaw[16];
    float base[16];
    float model[16];
    float walk_swing = sinf(walk_phase) * 0.72f * walk_amount;
    float swing_curve = sinf(swing * TC_PI);
    float right_arm = walk_swing + swing_curve * 1.25f;
    float left_arm = -walk_swing;
    float left_leg = -walk_swing * 0.85f;
    float right_leg = walk_swing * 0.85f;
    float shoulder[16];
    float arm_rotate[16];
    float arm_parent[16];
    float temporary[16];

    if (entities == NULL || renderer == NULL || player == NULL) {
        return;
    }

    tc_mat4_translate(base_translate, player->position[0],
                      player->position[1], player->position[2]);
    tc_mat4_rotate_y(base_yaw, -player->yaw);
    tc_mat4_mul(base, base_translate, base_yaw);

    part_matrix(model, base, 0.0f, 88.0f * TC_MODEL_UNIT, 0.0f,
                player->pitch * 0.8f, 0, TC_MODEL_UNIT, 14.0f, 0.0f, 14.0f);
    tc_renderer_draw_model(renderer, &entities->head, model);

    part_matrix(model, base, 0.0f, 44.0f * TC_MODEL_UNIT, 0.0f, 0.0f, 0,
                TC_MODEL_UNIT, 12.0f, 0.0f, 7.0f);
    tc_renderer_draw_model(renderer, &entities->torso, model);

    part_matrix(model, base, 18.0f * TC_MODEL_UNIT, 88.0f * TC_MODEL_UNIT,
                0.0f, right_arm, 1, TC_MODEL_UNIT, 6.0f, 0.0f, 6.0f);
    tc_renderer_draw_model(renderer, &entities->arm, model);

    part_matrix(model, base, -18.0f * TC_MODEL_UNIT, 88.0f * TC_MODEL_UNIT,
                0.0f, left_arm, 1, TC_MODEL_UNIT, 6.0f, 0.0f, 6.0f);
    tc_renderer_draw_model(renderer, &entities->arm, model);

    part_matrix(model, base, 7.0f * TC_MODEL_UNIT, 44.0f * TC_MODEL_UNIT,
                0.0f, left_leg, 1, TC_MODEL_UNIT, 6.0f, 0.0f, 6.0f);
    tc_renderer_draw_model(renderer, &entities->leg, model);

    part_matrix(model, base, -7.0f * TC_MODEL_UNIT, 44.0f * TC_MODEL_UNIT,
                0.0f, right_leg, 1, TC_MODEL_UNIT, 6.0f, 0.0f, 6.0f);
    tc_renderer_draw_model(renderer, &entities->leg, model);

    if (item == TC_ITEM_NONE || tc_item_get(item) == NULL) {
        return;
    }

    tc_mat4_translate(shoulder, 18.0f * TC_MODEL_UNIT,
                      88.0f * TC_MODEL_UNIT, 0.0f);
    tc_mat4_rotate_x(arm_rotate, right_arm);
    tc_mat4_mul(temporary, base, shoulder);
    tc_mat4_mul(arm_parent, temporary, arm_rotate);

    if (tc_item_block(item) != TC_BLOCK_AIR) {
        draw_item(entities, renderer, item, arm_parent, 0.12f, -0.48f, -0.30f,
                  0.45f, 0.12f, 0.38f * TC_MODEL_UNIT);
    } else {
        draw_item(entities, renderer, item, arm_parent, 0.12f, -0.54f, -0.30f,
                  0.25f, 0.08f, 0.84f * TC_MODEL_UNIT);
    }
}
