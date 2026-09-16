#include "fs.h"
#include "string.h"

static fs_entry_t files[FS_MAX_FILES];
static int file_count = 0;
static unsigned int next_untitled = 1;

static void copy_bounded(char* dest, const char* src, int max) {
    int i = 0;
    for (; i < max - 1 && src[i]; i++) dest[i] = src[i];
    dest[i] = '\0';
}

static void auto_name(char* out) {
    char num[8];
    int n = (int)next_untitled;
    int i = 0;

    if (n == 0) {
        num[i++] = '0';
    } else {
        char tmp[8];
        int t = 0;
        while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; }
        while (t > 0) num[i++] = tmp[--t];
    }
    num[i] = '\0';

    strcpy(out, "untitled_");
    strcat(out, num);
    strcat(out, ".txt");
}

static void add_entry(const char* name, unsigned int size_kb, const char* type, unsigned char is_dir, unsigned char location) {
    if (file_count >= FS_MAX_FILES) return;

    copy_bounded(files[file_count].name, name, FS_NAME_MAX);
    files[file_count].size_kb = size_kb;
    copy_bounded(files[file_count].type, type, FS_TYPE_MAX);
    files[file_count].is_dir = is_dir;
    files[file_count].location = location;
    file_count++;
}

void fs_init(void) {
    file_count = 0;
    next_untitled = 1;

    add_entry("kernel.bin", 42, "ELF32", 0, FS_LOC_LEFT);
    add_entry("drivers/",    0, "TREE",  1, FS_LOC_LEFT);
    add_entry("backup/",     0, "TREE",  1, FS_LOC_RIGHT);
    add_entry("notes.txt",   4, "TXT",   0, FS_LOC_RIGHT);
}

int fs_count(int location) {
    int c = 0;
    for (int i = 0; i < file_count; i++) {
        if (files[i].location == (unsigned char)location) c++;
    }
    return c;
}

int fs_get_index(int location, int nth) {
    int c = 0;
    for (int i = 0; i < file_count; i++) {
        if (files[i].location == (unsigned char)location) {
            if (c == nth) return i;
            c++;
        }
    }
    return -1;
}

const fs_entry_t* fs_get(int index) {
    if (index < 0 || index >= file_count) return 0;
    return &files[index];
}

int fs_create(int location, const char* name) {
    if (file_count >= FS_MAX_FILES) return -1;

    int idx = file_count;
    if (name && name[0]) {
        copy_bounded(files[idx].name, name, FS_NAME_MAX);
    } else {
        char generated[FS_NAME_MAX];
        auto_name(generated);
        copy_bounded(files[idx].name, generated, FS_NAME_MAX);
        next_untitled++;
    }

    files[idx].size_kb = 0;
    copy_bounded(files[idx].type, "TXT", FS_TYPE_MAX);
    files[idx].is_dir = 0;
    files[idx].location = (unsigned char)location;
    file_count++;
    return idx;
}

int fs_delete(int index) {
    if (index < 0 || index >= file_count) return -1;

    for (int i = index; i < file_count - 1; i++) {
        files[i] = files[i + 1];
    }
    file_count--;
    return 0;
}

int fs_rename(int index, const char* name) {
    if (index < 0 || index >= file_count) return -1;
    if (!name || !name[0]) return -1;

    copy_bounded(files[index].name, name, FS_NAME_MAX);
    return 0;
}

int fs_move(int index, int new_location) {
    if (index < 0 || index >= file_count) return -1;

    files[index].location = (unsigned char)new_location;
    return 0;
}

int fs_copy(int index, int new_location) {
    if (index < 0 || index >= file_count) return -1;
    if (file_count >= FS_MAX_FILES) return -1;

    int new_idx = file_count;
    files[new_idx] = files[index];
    files[new_idx].location = (unsigned char)new_location;
    file_count++;
    return new_idx;
}
