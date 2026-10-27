#include "thinkcraft/item.h"

static const tc_item_def item_registry[TC_ITEM_COUNT] = {
    {"GRASS", TC_BLOCK_GRASS, TC_ITEM_MAX_STACK, TC_TOOL_NONE,
     TC_TILE_GRASS_TOP, 0, 1.0f},
    {"DIRT", TC_BLOCK_DIRT, TC_ITEM_MAX_STACK, TC_TOOL_NONE,
     TC_TILE_DIRT, 0, 1.0f},
    {"STONE", TC_BLOCK_STONE, TC_ITEM_MAX_STACK, TC_TOOL_NONE,
     TC_TILE_STONE, 0, 1.0f},
    {"SAND", TC_BLOCK_SAND, TC_ITEM_MAX_STACK, TC_TOOL_NONE,
     TC_TILE_SAND, 0, 1.0f},
    {"WOODEN PICKAXE", TC_BLOCK_AIR, 1, TC_TOOL_PICKAXE,
     TC_TILE_WOODEN_PICKAXE, 59, 2.0f},
    {"WOODEN AXE", TC_BLOCK_AIR, 1, TC_TOOL_AXE,
     TC_TILE_WOODEN_AXE, 59, 2.0f},
    {"WOODEN SHOVEL", TC_BLOCK_AIR, 1, TC_TOOL_SHOVEL,
     TC_TILE_WOODEN_SHOVEL, 59, 2.0f},
    {"STONE PICKAXE", TC_BLOCK_AIR, 1, TC_TOOL_PICKAXE,
     TC_TILE_STONE_PICKAXE, 131, 4.0f},
    {"STONE AXE", TC_BLOCK_AIR, 1, TC_TOOL_AXE,
     TC_TILE_STONE_AXE, 131, 4.0f},
    {"STONE SHOVEL", TC_BLOCK_AIR, 1, TC_TOOL_SHOVEL,
     TC_TILE_STONE_SHOVEL, 131, 4.0f}
};

const tc_item_def *tc_item_get(uint16_t item) {
    if (item >= TC_ITEM_COUNT) {
        return NULL;
    }

    return &item_registry[item];
}

const char *tc_item_name(uint16_t item) {
    const tc_item_def *definition = tc_item_get(item);

    return definition != NULL ? definition->name : "EMPTY";
}

void tc_item_color(uint16_t item, float rgb[3]) {
    if (rgb == NULL) {
        return;
    }

    switch (item) {
        case TC_ITEM_GRASS:
            rgb[0] = 0.32f;
            rgb[1] = 0.60f;
            rgb[2] = 0.22f;
            break;
        case TC_ITEM_DIRT:
            rgb[0] = 0.52f;
            rgb[1] = 0.37f;
            rgb[2] = 0.25f;
            break;
        case TC_ITEM_STONE:
            rgb[0] = 0.50f;
            rgb[1] = 0.50f;
            rgb[2] = 0.53f;
            break;
        case TC_ITEM_SAND:
            rgb[0] = 0.86f;
            rgb[1] = 0.80f;
            rgb[2] = 0.61f;
            break;
        case TC_ITEM_WOODEN_PICKAXE:
        case TC_ITEM_WOODEN_AXE:
        case TC_ITEM_WOODEN_SHOVEL:
            rgb[0] = 0.62f;
            rgb[1] = 0.44f;
            rgb[2] = 0.24f;
            break;
        case TC_ITEM_STONE_PICKAXE:
        case TC_ITEM_STONE_AXE:
        case TC_ITEM_STONE_SHOVEL:
            rgb[0] = 0.68f;
            rgb[1] = 0.68f;
            rgb[2] = 0.72f;
            break;
        default:
            rgb[0] = 0.18f;
            rgb[1] = 0.18f;
            rgb[2] = 0.20f;
            break;
    }
}

int tc_item_is_valid(uint16_t item) {
    return item < TC_ITEM_COUNT;
}

uint16_t tc_item_for_block(uint8_t block) {
    switch (block) {
        case TC_BLOCK_GRASS:
            return TC_ITEM_GRASS;
        case TC_BLOCK_DIRT:
            return TC_ITEM_DIRT;
        case TC_BLOCK_STONE:
            return TC_ITEM_STONE;
        case TC_BLOCK_SAND:
            return TC_ITEM_SAND;
        default:
            return TC_ITEM_NONE;
    }
}

uint8_t tc_item_block(uint16_t item) {
    const tc_item_def *definition = tc_item_get(item);

    return definition != NULL ? definition->block : TC_BLOCK_AIR;
}

uint16_t tc_item_max_stack(uint16_t item) {
    const tc_item_def *definition = tc_item_get(item);

    return definition != NULL ? definition->max_stack : 0;
}

uint16_t tc_item_max_durability(uint16_t item) {
    const tc_item_def *definition = tc_item_get(item);

    return definition != NULL ? definition->durability : 0;
}

float tc_block_hardness(uint8_t block) {
    switch (block) {
        case TC_BLOCK_GRASS:
            return 0.6f;
        case TC_BLOCK_DIRT:
        case TC_BLOCK_SAND:
            return 0.5f;
        case TC_BLOCK_STONE:
            return 1.5f;
        default:
            return -1.0f;
    }
}

int tc_block_requires_tool(uint8_t block) {
    return block == TC_BLOCK_STONE;
}

uint16_t tc_item_mining_drop(uint8_t block, uint16_t tool) {
    const tc_item_def *definition;

    if (block == TC_BLOCK_GRASS) {
        return TC_ITEM_DIRT;
    }

    if (!tc_block_requires_tool(block)) {
        return tc_item_for_block(block);
    }

    definition = tc_item_get(tool);
    if (definition == NULL || definition->tool != TC_TOOL_PICKAXE) {
        return TC_ITEM_NONE;
    }

    return tc_item_for_block(block);
}

float tc_block_break_seconds(uint8_t block, uint16_t tool) {
    const tc_item_def *definition;
    uint8_t preferred;
    float hardness = tc_block_hardness(block);

    if (hardness < 0.0f) {
        return -1.0f;
    }

    switch (block) {
        case TC_BLOCK_STONE:
            preferred = TC_TOOL_PICKAXE;
            break;
        case TC_BLOCK_DIRT:
        case TC_BLOCK_GRASS:
        case TC_BLOCK_SAND:
            preferred = TC_TOOL_SHOVEL;
            break;
        default:
            preferred = TC_TOOL_NONE;
            break;
    }

    definition = tc_item_get(tool);
    if (definition != NULL && definition->tool == preferred) {
        hardness /= definition->mining_speed;
    }

    return hardness;
}
