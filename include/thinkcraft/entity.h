#ifndef THINKCRAFT_ENTITY_H
#define THINKCRAFT_ENTITY_H

#include "thinkcraft/item.h"
#include "thinkcraft/player.h"
#include "thinkcraft/renderer.h"

typedef struct {
    tc_render_mesh blocks[6];
    tc_render_mesh tools[TC_ITEM_COUNT];
    tc_render_mesh head;
    tc_render_mesh torso;
    tc_render_mesh arm;
    tc_render_mesh leg;
    tc_render_mesh hand_arm;
} tc_entity_renderer;

int tc_entity_renderer_init(tc_entity_renderer *entities,
                            tc_renderer *renderer);
void tc_entity_renderer_shutdown(tc_entity_renderer *entities,
                                 tc_renderer *renderer);
void tc_entity_draw_first_person(const tc_entity_renderer *entities,
                                 tc_renderer *renderer, const float eye[3],
                                 float yaw, float pitch, uint16_t item,
                                 float swing, float walk_amount,
                                 float walk_phase);
void tc_entity_draw_player(const tc_entity_renderer *entities,
                           tc_renderer *renderer, const tc_player *player,
                           uint16_t item, float swing, float walk_amount,
                           float walk_phase);

#endif
