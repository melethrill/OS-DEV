#ifndef FS_H
#define FS_H

#define FS_MAX_FILES 16
#define FS_NAME_MAX  24
#define FS_TYPE_MAX  8

typedef struct {
    char name[FS_NAME_MAX];
    unsigned int size_kb;
    char type[FS_TYPE_MAX];
    unsigned char is_dir;
} fs_entry_t;

void fs_init(void);
int fs_count(void);
const fs_entry_t* fs_get(int index);
int fs_create(const char* name);
int fs_delete(int index);
int fs_rename(int index, const char* name);

#endif
