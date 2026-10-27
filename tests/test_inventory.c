#include "thinkcraft/inventory.h"
#include "thinkcraft/item.h"

#include <math.h>
#include <stdio.h>

static int check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test_inventory: %s\n", message);
    }
    return condition;
}

static int test_items(void) {
    const tc_item_def *definition;
    int ok = 1;

    definition = tc_item_get(TC_ITEM_STONE);
    ok = ok && check(definition != NULL, "stone item is missing")
      && check(definition->block == TC_BLOCK_STONE, "stone block mapping")
      && check(definition->max_stack == TC_ITEM_MAX_STACK,
               "stone max stack")
      && check(tc_item_for_block(TC_BLOCK_GRASS) == TC_ITEM_GRASS,
               "grass item mapping")
      && check(tc_item_for_block(TC_BLOCK_BEDROCK) == TC_ITEM_NONE,
               "bedrock item mapping")
      && check(tc_item_block(TC_ITEM_STONE_PICKAXE) == TC_BLOCK_AIR,
               "tool block mapping")
      && check(tc_item_max_stack(TC_ITEM_STONE_PICKAXE) == 1,
               "tool max stack")
      && check(tc_item_max_durability(TC_ITEM_STONE_PICKAXE) == 131,
               "tool durability")
      && check(tc_item_mining_drop(TC_BLOCK_GRASS, TC_ITEM_NONE)
               == TC_ITEM_DIRT,
               "grass drop")
      && check(tc_item_mining_drop(TC_BLOCK_STONE, TC_ITEM_NONE)
               == TC_ITEM_NONE,
               "stone dropped without a pickaxe")
      && check(tc_item_mining_drop(TC_BLOCK_STONE, TC_ITEM_WOODEN_PICKAXE)
               == TC_ITEM_STONE,
               "stone did not drop with a pickaxe")
      && check(tc_item_mining_drop(TC_BLOCK_DIRT, TC_ITEM_NONE)
               == TC_ITEM_DIRT,
               "dirt drop");

    ok = ok && check(fabsf(tc_block_break_seconds(TC_BLOCK_STONE,
                                                   TC_ITEM_NONE)
                           - 1.5f) < 0.001f,
                     "stone hand break time")
      && check(fabsf(tc_block_break_seconds(TC_BLOCK_STONE,
                                            TC_ITEM_STONE_PICKAXE)
                     - 0.375f) < 0.001f,
               "stone pickaxe break time")
      && check(fabsf(tc_block_break_seconds(TC_BLOCK_DIRT,
                                            TC_ITEM_STONE_SHOVEL)
                     - 0.125f) < 0.001f,
               "shovel break time")
      && check(tc_block_break_seconds(TC_BLOCK_BEDROCK, TC_ITEM_NONE)
               < 0.0f,
               "bedrock is breakable");

    return ok;
}

static int test_add_remove(void) {
    tc_inventory inventory;
    int ok = 1;

    tc_inventory_init(&inventory);
    ok = ok && check(tc_item_stack_empty(&inventory.slots[0]),
                     "inventory did not start empty")
      && check(tc_item_stack_empty(&inventory.cursor),
               "cursor did not start empty")
      && check(inventory.selected == 0, "selected slot is not zero")
      && check(tc_inventory_add(&inventory, TC_ITEM_STONE, 100) == 0,
               "stone was not added")
      && check(inventory.slots[0].item == TC_ITEM_STONE
               && inventory.slots[0].count == 64,
               "first stone stack is wrong")
      && check(inventory.slots[1].item == TC_ITEM_STONE
               && inventory.slots[1].count == 36,
               "second stone stack is wrong")
      && check(tc_inventory_add(&inventory, TC_ITEM_STONE, 100) == 0,
               "second stone batch did not fit")
      && check(inventory.slots[1].count == 64
               && inventory.slots[2].count == 64
               && inventory.slots[3].count == 8,
               "second stone batch filled the wrong slots")
      && check(tc_inventory_remove(&inventory, 0, 10) == 10,
               "remove count is wrong")
      && check(inventory.slots[0].count == 54,
               "remove did not update the stack")
      && check(tc_inventory_remove(&inventory, 0, 100) == 54,
               "remove-all count is wrong")
      && check(tc_item_stack_empty(&inventory.slots[0]),
               "empty stack was not cleared");

    tc_inventory_select(&inventory, 20);
    ok = ok && check(inventory.selected == TC_ITEM_HOTBAR_COUNT - 1,
                     "selected slot was not clamped");
    tc_inventory_select(&inventory, -5);
    ok = ok && check(inventory.selected == 0,
                     "negative selected slot was not clamped");

    for (int index = 0; index < TC_INVENTORY_SLOT_COUNT; index++) {
        inventory.slots[index].item = TC_ITEM_STONE;
        inventory.slots[index].count = TC_ITEM_MAX_STACK;
        inventory.slots[index].durability = 0;
    }
    ok = ok && check(tc_inventory_add(&inventory, TC_ITEM_STONE, 1) == 1,
                     "full inventory did not report overflow");

    return ok;
}

static int test_starter(void) {
    tc_inventory inventory;
    int ok = 1;

    tc_inventory_init_starter(&inventory);
    ok = ok && check(inventory.slots[0].item == TC_ITEM_STONE_PICKAXE,
                     "starter pickaxe is missing")
      && check(inventory.slots[0].durability
               == tc_item_max_durability(TC_ITEM_STONE_PICKAXE),
               "starter pickaxe durability")
      && check(inventory.slots[1].item == TC_ITEM_STONE_AXE,
               "starter axe is missing")
      && check(inventory.slots[2].item == TC_ITEM_STONE_SHOVEL,
               "starter shovel is missing")
      && check(inventory.slots[3].item == TC_ITEM_GRASS
               && inventory.slots[3].count == TC_ITEM_MAX_STACK,
               "starter grass is missing")
      && check(inventory.slots[6].item == TC_ITEM_SAND,
               "starter sand is missing")
      && check(tc_item_stack_empty(&inventory.slots[7]),
               "starter slot 7 is not empty");

    return ok;
}

static int test_tools(void) {
    tc_inventory inventory;
    uint16_t broke = 0;
    int ok = 1;

    tc_inventory_init(&inventory);
    ok = ok && check(tc_inventory_add(&inventory, TC_ITEM_STONE_PICKAXE, 2)
                     == 0,
                     "tools were not added")
      && check(inventory.slots[0].item == TC_ITEM_STONE_PICKAXE
               && inventory.slots[1].item == TC_ITEM_STONE_PICKAXE,
               "tools did not use separate slots")
      && check(inventory.slots[0].durability == 131,
               "tool durability was not initialized")
      && check(tc_inventory_damage_selected(&inventory) == 0,
               "tool broke too early")
      && check(inventory.slots[0].durability == 130,
               "tool durability did not decrease");

    for (int index = 0; index < 200; index++) {
        broke = (uint16_t)(broke + tc_inventory_damage_selected(&inventory));
    }

    ok = ok && check(broke == 1, "tool did not break exactly once")
      && check(tc_item_stack_empty(&inventory.slots[0]),
               "broken tool was not removed");

    return ok;
}

static int test_clicks(void) {
    tc_inventory inventory;
    int ok = 1;

    tc_inventory_init(&inventory);
    inventory.slots[0].item = TC_ITEM_STONE;
    inventory.slots[0].count = 64;
    inventory.slots[0].durability = 0;

    ok = ok && check(tc_inventory_click(&inventory, 0, 1, 0),
                     "right click failed")
      && check(inventory.cursor.item == TC_ITEM_STONE
               && inventory.cursor.count == 32,
               "right click did not split the stack")
      && check(inventory.slots[0].count == 32,
               "right click left the wrong source count");

    ok = ok && check(tc_inventory_click(&inventory, 1, 1, 0),
                     "right click place failed")
      && check(inventory.slots[1].item == TC_ITEM_STONE
               && inventory.slots[1].count == 1,
               "right click did not place one item")
      && check(inventory.cursor.count == 31,
               "right click place did not consume one item");

    ok = ok && check(tc_inventory_click(&inventory, 1, 0, 0),
                     "left click merge failed")
      && check(inventory.slots[1].count == 32,
               "left click did not merge the cursor")
      && check(tc_item_stack_empty(&inventory.cursor),
               "cursor was not cleared after merging");

    inventory.slots[2].item = TC_ITEM_DIRT;
    inventory.slots[2].count = 10;
    ok = ok && check(tc_inventory_click(&inventory, 0, 0, 0),
                     "left click pick failed")
      && check(inventory.cursor.item == TC_ITEM_STONE
               && inventory.cursor.count == 32,
               "left click did not pick up the stack")
      && check(tc_inventory_click(&inventory, 2, 0, 0),
               "left click swap failed")
      && check(inventory.cursor.item == TC_ITEM_DIRT
               && inventory.cursor.count == 10,
               "cursor did not receive the swapped stack")
      && check(inventory.slots[2].item == TC_ITEM_STONE
               && inventory.slots[2].count == 32,
               "target did not receive the swapped stack");

    return ok;
}

static int test_quick_move_and_consume(void) {
    tc_inventory inventory;
    int ok = 1;

    tc_inventory_init(&inventory);
    inventory.slots[0].item = TC_ITEM_STONE;
    inventory.slots[0].count = 32;
    inventory.slots[10].item = TC_ITEM_STONE;
    inventory.slots[10].count = 40;

    ok = ok && check(tc_inventory_quick_move(&inventory, 0),
                     "quick move to backpack failed")
      && check(tc_item_stack_empty(&inventory.slots[0]),
               "quick move did not clear the source")
      && check(inventory.slots[10].count == 64,
               "quick move did not merge before using an empty slot")
      && check(inventory.slots[9].item == TC_ITEM_STONE
               && inventory.slots[9].count == 8,
               "quick move did not overflow into an empty slot")
      && check(tc_inventory_quick_move(&inventory, 9),
               "quick move to hotbar failed")
      && check(inventory.slots[0].count == 8,
               "quick move did not use a free hotbar slot")
      && check(tc_inventory_quick_move(&inventory, 10),
               "full stack quick move failed")
      && check(inventory.slots[0].count == 64,
               "full stack quick move did not top up the hotbar")
      && check(inventory.slots[1].count == 8,
               "full stack quick move did not move the remainder")
      && check(tc_item_stack_empty(&inventory.slots[10]),
               "full stack quick move left the source filled");

    tc_inventory_select(&inventory, 1);
    ok = ok && check(tc_inventory_consume_selected(&inventory, 3),
                     "selected item consume failed")
      && check(inventory.slots[1].count == 5,
               "consume changed the wrong count")
      && check(!tc_inventory_consume_selected(&inventory, 10),
               "oversized consume was accepted")
      && check(tc_inventory_consume_selected(&inventory, 5),
               "consume to empty failed")
      && check(tc_item_stack_empty(&inventory.slots[1]),
               "consumed stack was not cleared");

    return ok;
}

int main(void) {
    int ok = 1;

    ok = ok && test_items();
    ok = ok && test_add_remove();
    ok = ok && test_starter();
    ok = ok && test_tools();
    ok = ok && test_clicks();
    ok = ok && test_quick_move_and_consume();

    if (!ok) {
        return 1;
    }

    printf("test_inventory: all checks passed\n");
    return 0;
}
