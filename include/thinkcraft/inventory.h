#ifndef THINKCRAFT_INVENTORY_H
#define THINKCRAFT_INVENTORY_H

#include "thinkcraft/item.h"

typedef struct {
    uint16_t item;
    uint16_t count;
    uint16_t durability;
} tc_item_stack;

typedef struct {
    tc_item_stack slots[TC_INVENTORY_SLOT_COUNT];
    tc_item_stack cursor;
    int selected;
} tc_inventory;

static inline int tc_item_stack_empty(const tc_item_stack *stack) {
    return stack == NULL || stack->item == TC_ITEM_NONE || stack->count == 0;
}

void tc_inventory_init(tc_inventory *inventory);
void tc_inventory_init_starter(tc_inventory *inventory);
int tc_inventory_add(tc_inventory *inventory, uint16_t item, uint16_t count);
int tc_inventory_remove(tc_inventory *inventory, int slot, uint16_t count);
void tc_inventory_select(tc_inventory *inventory, int index);
tc_item_stack *tc_inventory_selected(tc_inventory *inventory);
const tc_item_stack *tc_inventory_selected_const(const tc_inventory *inventory);
int tc_inventory_quick_move(tc_inventory *inventory, int slot);
int tc_inventory_click(tc_inventory *inventory, int slot, int button,
                       int shift);
int tc_inventory_consume_selected(tc_inventory *inventory, uint16_t count);
int tc_inventory_damage_selected(tc_inventory *inventory);

#endif
