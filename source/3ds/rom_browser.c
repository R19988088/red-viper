#include "rom_browser.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef __3DS__
#include <dirent.h>
#include <3ds/archive.h>
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#endif

static bool has_rom_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".vb") == 0 || strcasecmp(dot, ".zip") == 0;
}

static int entry_compare(const void *left, const void *right) {
    const RomBrowserEntry *a = left;
    const RomBrowserEntry *b = right;
    if (a->is_directory != b->is_directory) return a->is_directory ? -1 : 1;
    int folded = strcasecmp(a->name, b->name);
    if (folded != 0) return folded;
    return strcmp(a->name, b->name);
}

static void free_entries(RomBrowserEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; i++) free(entries[i].name);
    free(entries);
}

static bool append_entry(RomBrowserEntry **entries, size_t *count, size_t *capacity,
                         const char *name, bool is_directory) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 8;
        RomBrowserEntry *grown = realloc(*entries, next * sizeof(**entries));
        if (!grown) return false;
        *entries = grown;
        *capacity = next;
    }
    size_t length = strlen(name) + 1;
    (*entries)[*count].name = malloc(length);
    if (!(*entries)[*count].name) return false;
    memcpy((*entries)[*count].name, name, length);
    (*entries)[*count].is_directory = is_directory;
    (*count)++;
    return true;
}

#ifndef __3DS__
static bool append_posix_entry(RomBrowserEntry **entries, size_t *count, size_t *capacity,
                               const char *path, const char *name) {
    char child[ROM_BROWSER_PATH_MAX];
    int written = snprintf(child, sizeof(child), "%s%s%s", path,
                           path[0] && path[strlen(path) - 1] == '/' ? "" : "/", name);
    if (written < 0 || (size_t)written >= sizeof(child)) return true;

    struct stat info;
    if (stat(child, &info) != 0) return true;
    if (S_ISDIR(info.st_mode)) return append_entry(entries, count, capacity, name, true);
    if (S_ISREG(info.st_mode) && has_rom_extension(name))
        return append_entry(entries, count, capacity, name, false);
    return true;
}
#endif

static bool scan_directory(const char *path, RomBrowserEntry **entries_out, size_t *count_out) {
    DIR *directory = opendir(path);
    if (!directory) return false;

    RomBrowserEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    bool success = true;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
#ifdef __3DS__
        archive_dir_t *state = (archive_dir_t *)directory->dirData->dirStruct;
        FS_DirectoryEntry *metadata = &state->entry_data[state->index];
        if (metadata->attributes & FS_ATTRIBUTE_HIDDEN) continue;
        bool is_directory = (metadata->attributes & FS_ATTRIBUTE_DIRECTORY) != 0;
        if (!is_directory && !has_rom_extension(entry->d_name)) continue;
        success = append_entry(&entries, &count, &capacity, entry->d_name, is_directory);
#else
        success = append_posix_entry(&entries, &count, &capacity, path, entry->d_name);
#endif
        if (!success) break;
    }
    closedir(directory);
    if (!success) {
        free_entries(entries, count);
        return false;
    }

    qsort(entries, count, sizeof(*entries), entry_compare);
    *entries_out = entries;
    *count_out = count;
    return true;
}

bool rom_browser_init(RomBrowserModel *model, const char *path) {
    if (!model || !path || !path[0] || strlen(path) >= sizeof(model->path)) return false;
    memset(model, 0, sizeof(*model));
    strcpy(model->path, path);
    return rom_browser_refresh(model);
}

void rom_browser_destroy(RomBrowserModel *model) {
    if (!model) return;
    free_entries(model->entries, model->count);
    memset(model, 0, sizeof(*model));
}

bool rom_browser_refresh(RomBrowserModel *model) {
    if (!model || !model->path[0]) return false;
    RomBrowserEntry *fresh = NULL;
    size_t fresh_count = 0;
    if (!scan_directory(model->path, &fresh, &fresh_count)) return false;

    const char *selected = NULL;
    if (model->cursor < model->count) selected = model->entries[model->cursor].name;
    size_t next_cursor = model->cursor;
    if (selected) {
        for (size_t i = 0; i < fresh_count; i++) {
            if (strcmp(selected, fresh[i].name) == 0) {
                next_cursor = i;
                break;
            }
        }
    }
    if (fresh_count && next_cursor >= fresh_count) next_cursor = fresh_count - 1;
    free_entries(model->entries, model->count);
    model->entries = fresh;
    model->count = fresh_count;
    model->cursor = fresh_count ? next_cursor : 0;
    model->generation++;
    return true;
}

size_t rom_browser_entry_count(const RomBrowserModel *model) {
    return model ? model->count : 0;
}

const char *rom_browser_entry_name(const RomBrowserModel *model, size_t index) {
    if (!model || index >= model->count) return NULL;
    return model->entries[index].name;
}

bool rom_browser_entry_is_directory(const RomBrowserModel *model, size_t index) {
    return model && index < model->count && model->entries[index].is_directory;
}

static bool set_path_and_refresh(RomBrowserModel *model, const char *next_path) {
    if (strlen(next_path) >= sizeof(model->path)) return false;
    char previous[sizeof(model->path)];
    strcpy(previous, model->path);
    strcpy(model->path, next_path);
    if (rom_browser_refresh(model)) return true;
    strcpy(model->path, previous);
    return false;
}

bool rom_browser_enter_directory(RomBrowserModel *model, size_t index) {
    if (!model || index >= model->count || !model->entries[index].is_directory) return false;
    char next_path[sizeof(model->path)];
    int written = snprintf(next_path, sizeof(next_path), "%s%s%s", model->path,
                           model->path[strlen(model->path) - 1] == '/' ? "" : "/",
                           model->entries[index].name);
    if (written < 0 || (size_t)written >= sizeof(next_path)) return false;
    return set_path_and_refresh(model, next_path);
}

bool rom_browser_go_up(RomBrowserModel *model) {
    if (!model || !model->path[0]) return false;
    char next_path[sizeof(model->path)];
    strcpy(next_path, model->path);
    size_t length = strlen(next_path);
    while (length > 1 && next_path[length - 1] == '/') next_path[--length] = 0;
    char *slash = strrchr(next_path, '/');
    if (!slash) return false;
    if (slash == next_path ||
        (strncmp(next_path, "sdmc:", 5) == 0 && slash == next_path + 5)) {
        next_path[1] = 0;
    } else {
        *slash = 0;
    }
    if (strcmp(next_path, model->path) == 0) return false;
    return set_path_and_refresh(model, next_path);
}
