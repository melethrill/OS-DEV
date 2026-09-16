#ifndef FS_H
#define FS_H

#define FS_MAX_FILES 32
#define FS_NAME_MAX  24
#define FS_TYPE_MAX  8

#define FS_LOC_LEFT  0
#define FS_LOC_RIGHT 1

typedef struct {
    char name[FS_NAME_MAX];
    unsigned int size_kb;
    char type[FS_TYPE_MAX];
    unsigned char is_dir;
    unsigned char location;
} fs_entry_t;

void fs_init(void);

int fs_count(int location);
int fs_get_index(int location, int nth);
const fs_entry_t* fs_get(int index);

int fs_create(int location, const char* name);
int fs_delete(int index);
int fs_rename(int index, const char* name);
int fs_move(int index, int new_location);
int fs_copy(int index, int new_location);

#endif
