#ifndef ROM_BROWSER_H
#define ROM_BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#define ROM_BROWSER_PATH_MAX 1024

typedef struct {
    char *name;
    bool is_directory;
} RomBrowserEntry;

typedef struct {
    char path[ROM_BROWSER_PATH_MAX];
    RomBrowserEntry *entries;
    size_t count;
    size_t cursor;
    unsigned long generation;
} RomBrowserModel;

bool rom_browser_init(RomBrowserModel *model, const char *path);
void rom_browser_destroy(RomBrowserModel *model);
bool rom_browser_refresh(RomBrowserModel *model);
size_t rom_browser_entry_count(const RomBrowserModel *model);
const char *rom_browser_entry_name(const RomBrowserModel *model, size_t index);
bool rom_browser_entry_is_directory(const RomBrowserModel *model, size_t index);
bool rom_browser_enter_directory(RomBrowserModel *model, size_t index);
bool rom_browser_go_up(RomBrowserModel *model);

#endif
