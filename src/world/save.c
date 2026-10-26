#include "thinkcraft/save.h"
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
        && data->selected_hotbar < 4;
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
                  const tc_player *player, int selected_hotbar) {
    unsigned char header[40];
    char temporary[4096];
    FILE *file;
    int written;

    if (path == NULL || world == NULL || player == NULL
        || selected_hotbar < 0 || selected_hotbar >= 4
        || world->edit_count > UINT32_MAX
        || world->edit_count > TC_SAVE_MAX_EDITS
        || strlen(path) + 5 >= sizeof(temporary)) {
        return 0;
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
    write_i32(header + 16, selected_hotbar);
    write_f32(header + 20, player->position[0]);
    write_f32(header + 24, player->position[1]);
    write_f32(header + 28, player->position[2]);
    write_f32(header + 32, player->yaw);
    write_f32(header + 36, player->pitch);

    file = fopen(temporary, "wb");
    if (file == NULL) {
        tc_log_error("could not open save file %s: %s", temporary,
                     strerror(errno));
        return 0;
    }

    written = write_bytes(file, header, sizeof(header));
    for (size_t index = 0; written && index < world->edit_count; index++) {
        const tc_world_edit *edit = &world->edits[index];
        unsigned char record[13];

        write_i32(record, edit->x);
        write_i32(record + 4, edit->y);
        write_i32(record + 8, edit->z);
        record[12] = edit->block;
        written = write_bytes(file, record, sizeof(record));
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
    unsigned char header[40];
    unsigned char record[13];
    FILE *file;
    uint32_t magic;
    uint32_t version;
    uint32_t edit_count;

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

    if (!read_bytes(file, header, sizeof(header))) {
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
    if (version != TC_SAVE_VERSION) {
        tc_log_error("save file %s uses unsupported version %u", path,
                     version);
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

    if (edit_count > TC_SAVE_MAX_EDITS || !valid_position(data)) {
        tc_log_error("save file %s contains invalid header data", path);
        fclose(file);
        return 0;
    }

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

        if (!read_bytes(file, record, sizeof(record))) {
            tc_log_error("save file %s is truncated", path);
            tc_save_data_shutdown(data);
            fclose(file);
            return 0;
        }

        edit->x = read_i32(record);
        edit->y = read_i32(record + 4);
        edit->z = read_i32(record + 8);
        edit->block = record[12];

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
