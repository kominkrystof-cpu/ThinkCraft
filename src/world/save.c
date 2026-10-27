#include "thinkcraft/save.h"
#include "thinkcraft/item.h"
#include "thinkcraft/log.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static uint32_t read_u32(const unsigned char *bytes) {
    return (uint32_t)bytes[0]
         | ((uint32_t)bytes[1] << 8)
         | ((uint32_t)bytes[2] << 16)
         | ((uint32_t)bytes[3] << 24);
}

static int32_t read_i32(const unsigned char *bytes) {
    return (int32_t)read_u32(bytes);
}

static float read_f32(const unsigned char *bytes) {
    uint32_t bits = read_u32(bytes);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_u32(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8) & 0xffu);
    bytes[2] = (unsigned char)((value >> 16) & 0xffu);
    bytes[3] = (unsigned char)((value >> 24) & 0xffu);
}

static void write_i32(unsigned char *bytes, int32_t value) {
    write_u32(bytes, (uint32_t)value);
}

static void write_f32(unsigned char *bytes, float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    write_u32(bytes, bits);
}

static uint16_t read_u16(const unsigned char *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void write_u16(unsigned char *bytes, uint16_t value) {
    bytes[0] = (unsigned char)(value & 0xffu);
    bytes[1] = (unsigned char)((value >> 8) & 0xffu);
}

#define TC_SAVE_HEADER_BYTES 64
#define TC_SAVE_V3_HEADER_BYTES 48
#define TC_SAVE_STACK_BYTES 8

static int stack_valid(const tc_item_stack *stack) {
    uint16_t max_stack;
    uint16_t max_durability;

    if (stack->item == TC_ITEM_NONE || stack->count == 0) {
        return stack->item == TC_ITEM_NONE && stack->count == 0
            && stack->durability == 0;
    }
    if (!tc_item_is_valid(stack->item)) {
        return 0;
    }

    max_stack = tc_item_max_stack(stack->item);
    max_durability = tc_item_max_durability(stack->item);
    return stack->count > 0 && stack->count <= max_stack
        && stack->durability <= max_durability;
}

static void write_stack(unsigned char *record, const tc_item_stack *stack) {
    write_u16(record, stack->item);
    write_u16(record + 2, stack->count);
    write_u16(record + 4, stack->durability);
    write_u16(record + 6, 0);
}

static void read_stack(const unsigned char *record, tc_item_stack *stack) {
    stack->item = read_u16(record);
    stack->count = read_u16(record + 2);
    stack->durability = read_u16(record + 4);
}

static int write_bytes(FILE *file, const void *bytes, size_t count) {
    return fwrite(bytes, 1, count, file) == count;
}

static int read_bytes(FILE *file, void *bytes, size_t count) {
    return fread(bytes, 1, count, file) == count;
}

static int valid_position(const tc_save_data *data) {
    return isfinite(data->position[0])
        && isfinite(data->position[1])
        && isfinite(data->position[2])
        && isfinite(data->yaw)
        && isfinite(data->pitch)
        && data->position[1] >= 0.0f
        && data->position[1] < (float)TC_CHUNK_SIZE_Y
        && data->selected_hotbar >= 0
        && data->selected_hotbar < TC_ITEM_HOTBAR_COUNT;
}

int tc_save_ensure_directory(const char *path) {
    char buffer[4096];
    char *slash;
    size_t length;

    if (path == NULL) {
        return 0;
    }

    length = strlen(path);
    if (length == 0 || length >= sizeof(buffer)) {
        return 0;
    }

    memcpy(buffer, path, length + 1);
    slash = strrchr(buffer, '/');
    if (slash == NULL) {
        return 1;
    }

    *slash = '\0';
    for (char *cursor = buffer + 1; *cursor != '\0'; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        *cursor = '\0';
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
            return 0;
        }
        *cursor = '/';
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        return 0;
    }

    return 1;
}

int tc_save_write(const char *path, const tc_world *world,
                  const tc_player *player, const tc_inventory *inventory,
                  tc_game_mode game_mode) {
    unsigned char header[TC_SAVE_HEADER_BYTES];
    unsigned char record[TC_SAVE_STACK_BYTES];
    char temporary[4096];
    FILE *file;
    int written;

    if (path == NULL || world == NULL || player == NULL || inventory == NULL
        || inventory->selected < 0
        || inventory->selected >= TC_ITEM_HOTBAR_COUNT
        || game_mode < 0 || game_mode >= TC_GAME_MODE_COUNT
        || !isfinite(player->survival.health)
        || !isfinite(player->survival.hunger)
        || !isfinite(player->survival.exhaustion)
        || player->survival.health < 0.0f
        || player->survival.health > TC_SURVIVAL_MAX_HEALTH
        || player->survival.hunger < 0.0f
        || player->survival.hunger > TC_SURVIVAL_MAX_HUNGER
        || player->survival.exhaustion < 0.0f
        || player->survival.exhaustion > 100.0f
        || !stack_valid(&inventory->cursor)
        || world->edit_count > UINT32_MAX
        || world->edit_count > TC_SAVE_MAX_EDITS
        || strlen(path) + 5 >= sizeof(temporary)) {
        return 0;
    }

    for (int index = 0; index < TC_INVENTORY_SLOT_COUNT; index++) {
        if (!stack_valid(&inventory->slots[index])) {
            return 0;
        }
    }

    if (!tc_save_ensure_directory(path)) {
        tc_log_error("could not create save directory for %s", path);
        return 0;
    }

    snprintf(temporary, sizeof(temporary), "%s.tmp", path);

    header[0] = 'T';
    header[1] = 'C';
    header[2] = 'S';
    header[3] = 'V';
    write_u32(header + 4, TC_SAVE_VERSION);
    write_u32(header + 8, world->seed);
    write_u32(header + 12, (uint32_t)world->edit_count);
    write_i32(header + 16, inventory->selected);
    write_f32(header + 20, player->position[0]);
    write_f32(header + 24, player->position[1]);
    write_f32(header + 28, player->position[2]);
    write_f32(header + 32, player->yaw);
    write_f32(header + 36, player->pitch);
    write_u32(header + 40, (uint32_t)game_mode);
    write_u32(header + 44, 0);
    write_f32(header + 48, player->survival.health);
    write_f32(header + 52, player->survival.hunger);
    write_f32(header + 56, player->survival.exhaustion);
    write_u32(header + 60, 0);

    file = fopen(temporary, "wb");
    if (file == NULL) {
        tc_log_error("could not open save file %s: %s", temporary,
                     strerror(errno));
        return 0;
    }

    written = write_bytes(file, header, sizeof(header));
    for (int index = 0; written && index < TC_INVENTORY_SLOT_COUNT; index++) {
        write_stack(record, &inventory->slots[index]);
        written = write_bytes(file, record, sizeof(record));
    }
    if (written) {
        write_stack(record, &inventory->cursor);
        written = write_bytes(file, record, sizeof(record));
    }
    for (size_t index = 0; written && index < world->edit_count; index++) {
        const tc_world_edit *edit = &world->edits[index];
        unsigned char edit_record[13];

        write_i32(edit_record, edit->x);
        write_i32(edit_record + 4, edit->y);
        write_i32(edit_record + 8, edit->z);
        edit_record[12] = edit->block;
        written = write_bytes(file, edit_record, sizeof(edit_record));
    }

    if (fclose(file) != 0) {
        written = 0;
    }

    if (!written) {
        remove(temporary);
        tc_log_error("failed to write save file %s", path);
        return 0;
    }

    if (rename(temporary, path) != 0) {
        remove(temporary);
        tc_log_error("failed to replace save file %s: %s", path,
                     strerror(errno));
        return 0;
    }

    return 1;
}

int tc_save_read(const char *path, tc_save_data *data) {
    unsigned char header[TC_SAVE_HEADER_BYTES];
    unsigned char record[TC_SAVE_STACK_BYTES];
    unsigned char edit_record[13];
    FILE *file;
    uint32_t magic;
    uint32_t version;
    uint32_t edit_count;
    uint32_t game_mode;
    size_t header_bytes;

    if (path == NULL || data == NULL) {
        return 0;
    }

    memset(data, 0, sizeof(*data));

    file = fopen(path, "rb");
    if (file == NULL) {
        tc_log_error("could not open save file %s: %s", path,
                     strerror(errno));
        return 0;
    }

    if (!read_bytes(file, header, 8)) {
        tc_log_error("save file %s is truncated", path);
        fclose(file);
        return 0;
    }

    magic = read_u32(header);
    version = read_u32(header + 4);
    if (magic != 0x56534354u) {
        tc_log_error("save file %s has an invalid magic", path);
        fclose(file);
        return 0;
    }
    if (version == 3u) {
        header_bytes = TC_SAVE_V3_HEADER_BYTES;
    } else if (version == TC_SAVE_VERSION) {
        header_bytes = TC_SAVE_HEADER_BYTES;
    } else {
        tc_log_error("save file %s uses unsupported version %u", path,
                     version);
        fclose(file);
        return 0;
    }

    if (!read_bytes(file, header + 8, header_bytes - 8)) {
        tc_log_error("save file %s is truncated", path);
        fclose(file);
        return 0;
    }

    data->seed = read_u32(header + 8);
    edit_count = read_u32(header + 12);
    data->selected_hotbar = read_i32(header + 16);
    data->position[0] = read_f32(header + 20);
    data->position[1] = read_f32(header + 24);
    data->position[2] = read_f32(header + 28);
    data->yaw = read_f32(header + 32);
    data->pitch = read_f32(header + 36);
    game_mode = read_u32(header + 40);

    data->health = TC_SURVIVAL_MAX_HEALTH;
    data->hunger = TC_SURVIVAL_MAX_HUNGER;
    data->exhaustion = 0.0f;
    if (version >= 4u) {
        data->health = read_f32(header + 48);
        data->hunger = read_f32(header + 52);
        data->exhaustion = read_f32(header + 56);
    }

    if (edit_count > TC_SAVE_MAX_EDITS || !valid_position(data)
        || game_mode >= TC_GAME_MODE_COUNT
        || !isfinite(data->health)
        || !isfinite(data->hunger)
        || !isfinite(data->exhaustion)
        || data->health < 0.0f
        || data->health > TC_SURVIVAL_MAX_HEALTH
        || data->hunger < 0.0f
        || data->hunger > TC_SURVIVAL_MAX_HUNGER
        || data->exhaustion < 0.0f
        || data->exhaustion > 100.0f) {
        tc_log_error("save file %s contains invalid header data", path);
        fclose(file);
        return 0;
    }
    data->game_mode = (tc_game_mode)game_mode;

    for (int index = 0; index < TC_INVENTORY_SLOT_COUNT; index++) {
        if (!read_bytes(file, record, sizeof(record))) {
            tc_log_error("save file %s is truncated", path);
            fclose(file);
            return 0;
        }
        read_stack(record, &data->inventory.slots[index]);
        if (!stack_valid(&data->inventory.slots[index])) {
            tc_log_error("save file %s contains an invalid item stack", path);
            fclose(file);
            return 0;
        }
    }
    if (!read_bytes(file, record, sizeof(record))) {
        tc_log_error("save file %s is truncated", path);
        fclose(file);
        return 0;
    }
    read_stack(record, &data->inventory.cursor);
    if (!stack_valid(&data->inventory.cursor)) {
        tc_log_error("save file %s contains an invalid cursor stack", path);
        fclose(file);
        return 0;
    }
    data->inventory.selected = data->selected_hotbar;

    if (edit_count > 0) {
        data->edits = malloc((size_t)edit_count * sizeof(*data->edits));
        if (data->edits == NULL) {
            tc_log_error("could not allocate save edits");
            fclose(file);
            return 0;
        }
    }

    for (uint32_t index = 0; index < edit_count; index++) {
        tc_world_edit *edit = &data->edits[index];

        if (!read_bytes(file, edit_record, sizeof(edit_record))) {
            tc_log_error("save file %s is truncated", path);
            tc_save_data_shutdown(data);
            fclose(file);
            return 0;
        }

        edit->x = read_i32(edit_record);
        edit->y = read_i32(edit_record + 4);
        edit->z = read_i32(edit_record + 8);
        edit->block = edit_record[12];

        if (edit->y < 0 || edit->y >= TC_CHUNK_SIZE_Y
            || (edit->y < TC_WORLD_BEDROCK_LAYERS
                && edit->block != TC_BLOCK_BEDROCK)) {
            tc_log_error("save file %s contains an invalid block edit", path);
            tc_save_data_shutdown(data);
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    data->edit_count = edit_count;
    return 1;
}

void tc_save_data_shutdown(tc_save_data *data) {
    if (data != NULL) {
        free(data->edits);
        memset(data, 0, sizeof(*data));
    }
}
