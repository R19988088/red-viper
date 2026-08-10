#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rom_browser.h"

static void touch_file(const char *path) {
    int fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert(fd >= 0);
    close(fd);
}

static bool has_entry(const RomBrowserModel *model, const char *name) {
    for (size_t i = 0; i < rom_browser_entry_count(model); i++)
        if (strcmp(rom_browser_entry_name(model, i), name) == 0) return true;
    return false;
}

int main(void) {
    char root[] = "/tmp/rom-browser-test-XXXXXX";
    assert(mkdtemp(root));
    char path[ROM_BROWSER_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", root, "old.vb");
    touch_file(path);
    snprintf(path, sizeof(path), "%s/%s", root, "OLD.ZIP");
    touch_file(path);
    snprintf(path, sizeof(path), "%s/%s", root, "subdir");
    assert(mkdir(path, 0700) == 0);
    snprintf(path, sizeof(path), "%s/%s", root, ".hidden.vb");
    touch_file(path);
    snprintf(path, sizeof(path), "%s/%s", root, "notes.txt");
    touch_file(path);

    RomBrowserModel model;
    assert(rom_browser_init(&model, root));
    assert(model.generation == 1);
    assert(rom_browser_entry_count(&model) == 3);
    assert(rom_browser_entry_is_directory(&model, 0));
    assert(strcmp(rom_browser_entry_name(&model, 1), "old.vb") == 0);
    assert(strcmp(rom_browser_entry_name(&model, 2), "OLD.ZIP") == 0);

    model.cursor = 1;
    snprintf(path, sizeof(path), "%s/%s", root, "Dragon Hopper (Japan).zip");
    touch_file(path);
    assert(rom_browser_refresh(&model));
    assert(model.generation == 2);
    assert(has_entry(&model, "Dragon Hopper (Japan).zip"));
    assert(strcmp(rom_browser_entry_name(&model, model.cursor), "old.vb") == 0);

    model.cursor = 99;
    assert(rom_browser_refresh(&model));
    assert(model.cursor == rom_browser_entry_count(&model) - 1);

    char missing[ROM_BROWSER_PATH_MAX];
    snprintf(missing, sizeof(missing), "%s/%s", root, "missing");
    strcpy(model.path, missing);
    size_t count_before = model.count;
    unsigned long generation_before = model.generation;
    assert(!rom_browser_refresh(&model));
    assert(model.count == count_before);
    assert(model.generation == generation_before);

    strcpy(model.path, root);
    model.cursor = 0;
    assert(rom_browser_enter_directory(&model, 0));
    assert(strcmp(model.path + strlen(model.path) - 6, "subdir") == 0);
    assert(rom_browser_go_up(&model));
    assert(strcmp(model.path, root) == 0);

    rom_browser_destroy(&model);
    assert(rom_browser_entry_count(&model) == 0);
    snprintf(path, sizeof(path), "%s/%s", root, "subdir");
    rmdir(path);
    snprintf(path, sizeof(path), "%s/%s", root, "old.vb"); unlink(path);
    snprintf(path, sizeof(path), "%s/%s", root, "OLD.ZIP"); unlink(path);
    snprintf(path, sizeof(path), "%s/%s", root, "Dragon Hopper (Japan).zip"); unlink(path);
    snprintf(path, sizeof(path), "%s/%s", root, ".hidden.vb"); unlink(path);
    snprintf(path, sizeof(path), "%s/%s", root, "notes.txt"); unlink(path);
    rmdir(root);
    return 0;
}
