#include "thinkcraft/inventory.h"

#include <string.h>

static void clear_stack(tc_item_stack *stack) {
    stack->item = TC_ITEM_NONE;
    stack->count = 0;
    stack->durability = 0;
}

static void create_stack(tc_item_stack *stack, uint16_t item, uint16_t count) {
    stack->item = item;
    stack->count = count;
    stack->durability = tc_item_max_durability(item);
}

static void copy_stack(tc_item_stack *destination,
                       const tc_item_stack *source, uint16_t count) {
    destination->item = source->item;
    destination->count = count;
    destination->durability = source->durability;
}

void tc_inventory_init(tc_inventory *inventory) {
    if (inventory == NULL) {
        return;
    }

    memset(inventory, 0, sizeof(*inventory));
    for (int index = 0; index < TC_INVENTORY_SLOT_COUNT; index++) {
        clear_stack(&inventory->slots[index]);
    }
    clear_stack(&inventory->cursor);
}

void tc_inventory_init_starter(tc_inventory *inventory) {
    if (inventory == NULL) {
        return;
    }

    tc_inventory_init(inventory);
    tc_inventory_add(inventory, TC_ITEM_STONE_PICKAXE, 1);
    tc_inventory_add(inventory, TC_ITEM_STONE_AXE, 1);
    tc_inventory_add(inventory, TC_ITEM_STONE_SHOVEL, 1);
    tc_inventory_add(inventory, TC_ITEM_GRASS, TC_ITEM_MAX_STACK);
    tc_inventory_add(inventory, TC_ITEM_DIRT, TC_ITEM_MAX_STACK);
    tc_inventory_add(inventory, TC_ITEM_STONE, TC_ITEM_MAX_STACK);
    tc_inventory_add(inventory, TC_ITEM_SAND, TC_ITEM_MAX_STACK);
}

int tc_inventory_add(tc_inventory *inventory, uint16_t item, uint16_t count) {
    uint16_t max_stack;

    if (inventory == NULL || !tc_item_is_valid(item) || count == 0) {
        return count;
    }

    max_stack = tc_item_max_stack(item);
    if (max_stack == 0) {
        return count;
    }

    if (max_stack > 1) {
        for (int index = 0; index < TC_INVENTORY_SLOT_COUNT && count > 0;
             index++) {
            tc_item_stack *slot = &inventory->slots[index];
            uint16_t room;

            if (slot->item != item || slot->count >= max_stack) {
                continue;
            }

            room = (uint16_t)(max_stack - slot->count);
            if (room > count) {
                room = count;
            }
            slot->count = (uint16_t)(slot->count + room);
            count = (uint16_t)(count - room);
        }
    }

    for (int index = 0; index < TC_INVENTORY_SLOT_COUNT && count > 0; index++) {
        tc_item_stack *slot = &inventory->slots[index];
        uint16_t amount;

        if (!tc_item_stack_empty(slot)) {
            continue;
        }

        amount = count < max_stack ? count : max_stack;
        create_stack(slot, item, amount);
        count = (uint16_t)(count - amount);
    }

    return count;
}

int tc_inventory_remove(tc_inventory *inventory, int slot, uint16_t count) {
    tc_item_stack *stack;
    uint16_t removed;

    if (inventory == NULL || slot < 0 || slot >= TC_INVENTORY_SLOT_COUNT
        || count == 0) {
        return 0;
    }

    stack = &inventory->slots[slot];
    if (tc_item_stack_empty(stack)) {
        return 0;
    }

    removed = count < stack->count ? count : stack->count;
    stack->count = (uint16_t)(stack->count - removed);
    if (stack->count == 0) {
        clear_stack(stack);
    }

    return removed;
}

void tc_inventory_select(tc_inventory *inventory, int index) {
    if (inventory == NULL) {
        return;
    }

    if (index < 0) {
        index = 0;
    } else if (index >= TC_ITEM_HOTBAR_COUNT) {
        index = TC_ITEM_HOTBAR_COUNT - 1;
    }

    inventory->selected = index;
}

tc_item_stack *tc_inventory_selected(tc_inventory *inventory) {
    if (inventory == NULL) {
        return NULL;
    }

    return &inventory->slots[inventory->selected];
}

const tc_item_stack *tc_inventory_selected_const(const tc_inventory *inventory) {
    if (inventory == NULL) {
        return NULL;
    }

    return &inventory->slots[inventory->selected];
}

int tc_inventory_quick_move(tc_inventory *inventory, int slot) {
    tc_item_stack *source;
    int start;
    int end;
    int moved = 0;

    if (inventory == NULL || slot < 0 || slot >= TC_INVENTORY_SLOT_COUNT) {
        return 0;
    }

    source = &inventory->slots[slot];
    if (tc_item_stack_empty(source)) {
        return 0;
    }

    start = slot < TC_ITEM_HOTBAR_COUNT ? TC_ITEM_HOTBAR_COUNT : 0;
    end = slot < TC_ITEM_HOTBAR_COUNT ? TC_INVENTORY_SLOT_COUNT
                                      : TC_ITEM_HOTBAR_COUNT;

    if (tc_item_max_stack(source->item) > 1) {
        for (int index = start; index < end; index++) {
            tc_item_stack *target = &inventory->slots[index];
            uint16_t max_stack = tc_item_max_stack(source->item);
            uint16_t room;

            if (target->item != source->item
                || target->count >= max_stack) {
                continue;
            }

            room = (uint16_t)(max_stack - target->count);
            if (room > source->count) {
                room = source->count;
            }
            target->count = (uint16_t)(target->count + room);
            source->count = (uint16_t)(source->count - room);
            moved = 1;
            if (source->count == 0) {
                clear_stack(source);
                return 1;
            }
        }
    }

    for (int index = start; index < end; index++) {
        tc_item_stack *target = &inventory->slots[index];

        if (!tc_item_stack_empty(target)) {
            continue;
        }

        *target = *source;
        clear_stack(source);
        return 1;
    }

    return moved;
}

int tc_inventory_click(tc_inventory *inventory, int slot, int button,
                       int shift) {
    tc_item_stack *target;
    tc_item_stack *cursor;

    if (inventory == NULL || slot < 0 || slot >= TC_INVENTORY_SLOT_COUNT
        || (button != 0 && button != 1)) {
        return 0;
    }

    if (shift) {
        return tc_inventory_quick_move(inventory, slot);
    }

    target = &inventory->slots[slot];
    cursor = &inventory->cursor;

    if (tc_item_stack_empty(cursor)) {
        uint16_t amount;

        if (tc_item_stack_empty(target)) {
            return 1;
        }

        amount = button == 0 ? target->count
                             : (uint16_t)((target->count + 1) / 2);
        copy_stack(cursor, target, amount);
        target->count = (uint16_t)(target->count - amount);
        if (target->count == 0) {
            clear_stack(target);
        }
        return 1;
    }

    if (tc_item_stack_empty(target)) {
        if (button == 0) {
            *target = *cursor;
            clear_stack(cursor);
        } else {
            copy_stack(target, cursor, 1);
            cursor->count = (uint16_t)(cursor->count - 1);
            if (cursor->count == 0) {
                clear_stack(cursor);
            }
        }
        return 1;
    }

    if (target->item == cursor->item) {
        uint16_t max_stack = tc_item_max_stack(target->item);
        uint16_t room;

        if (max_stack <= 1 || target->count >= max_stack) {
            if (button == 0) {
                tc_item_stack temporary = *cursor;

                *cursor = *target;
                *target = temporary;
            }
            return 1;
        }

        room = (uint16_t)(max_stack - target->count);
        if (button == 1) {
            room = 1;
        }
        if (room > cursor->count) {
            room = cursor->count;
        }
        target->count = (uint16_t)(target->count + room);
        cursor->count = (uint16_t)(cursor->count - room);
        if (cursor->count == 0) {
            clear_stack(cursor);
        }
        return 1;
    }

    if (button == 0) {
        tc_item_stack temporary = *cursor;

        *cursor = *target;
        *target = temporary;
    }

    return 1;
}

int tc_inventory_consume_selected(tc_inventory *inventory, uint16_t count) {
    tc_item_stack *selected;

    if (inventory == NULL || count == 0) {
        return 0;
    }

    selected = tc_inventory_selected(inventory);
    if (tc_item_stack_empty(selected) || selected->count < count) {
        return 0;
    }

    selected->count = (uint16_t)(selected->count - count);
    if (selected->count == 0) {
        clear_stack(selected);
    }

    return 1;
}

int tc_inventory_damage_selected(tc_inventory *inventory) {
    tc_item_stack *selected;

    if (inventory == NULL) {
        return 0;
    }

    selected = tc_inventory_selected(inventory);
    if (tc_item_stack_empty(selected) || selected->durability == 0) {
        return 0;
    }

    selected->durability--;
    if (selected->durability == 0) {
        clear_stack(selected);
        return 1;
    }

    return 0;
}
