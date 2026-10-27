#ifndef THINKCRAFT_ITEM_H
#define THINKCRAFT_ITEM_H

#include "thinkcraft/block.h"
#include "thinkcraft/texture.h"

#include <stddef.h>
#include <stdint.h>

#define TC_ITEM_NONE 0xFFFFu
#define TC_ITEM_MAX_STACK 64
#define TC_ITEM_HOTBAR_COUNT 9
#define TC_INVENTORY_SLOT_COUNT 36
#define TC_INVENTORY_BACKPACK_START TC_ITEM_HOTBAR_COUNT

typedef enum {
    TC_ITEM_GRASS = 0,
    TC_ITEM_DIRT,
    TC_ITEM_STONE,
    TC_ITEM_SAND,
    TC_ITEM_WOODEN_PICKAXE,
    TC_ITEM_WOODEN_AXE,
    TC_ITEM_WOODEN_SHOVEL,
    TC_ITEM_STONE_PICKAXE,
    TC_ITEM_STONE_AXE,
    TC_ITEM_STONE_SHOVEL,
    TC_ITEM_COUNT
} tc_item_id;

typedef enum {
    TC_TOOL_NONE = 0,
    TC_TOOL_PICKAXE,
    TC_TOOL_AXE,
    TC_TOOL_SHOVEL
} tc_tool_type;

typedef struct {
    const char *name;
    uint8_t block;
    uint8_t max_stack;
    uint8_t tool;
    uint8_t icon_tile;
    uint16_t durability;
    float mining_speed;
} tc_item_def;

const tc_item_def *tc_item_get(uint16_t item);
const char *tc_item_name(uint16_t item);
void tc_item_color(uint16_t item, float rgb[3]);
int tc_item_is_valid(uint16_t item);
uint16_t tc_item_for_block(uint8_t block);
uint8_t tc_item_block(uint16_t item);
uint16_t tc_item_max_stack(uint16_t item);
uint16_t tc_item_max_durability(uint16_t item);
uint16_t tc_item_mining_drop(uint8_t block, uint16_t tool);

float tc_block_hardness(uint8_t block);
float tc_block_break_seconds(uint8_t block, uint16_t tool);
int tc_block_requires_tool(uint8_t block);

#endif
