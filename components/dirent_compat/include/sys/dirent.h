#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t dd_vfs_idx;
    uint16_t dd_rsv;
} DIR;

struct dirent {
    ino_t d_ino;
    uint8_t d_type;
#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_REG
#define DT_REG 1
#endif
#ifndef DT_DIR
#define DT_DIR 2
#endif
#if defined(__BSD_VISIBLE)
#ifndef MAXNAMLEN
#define MAXNAMLEN 255
#endif
    char d_name[MAXNAMLEN + 1];
#else
    char d_name[256];
#endif
};

DIR *opendir(const char *name);
struct dirent *readdir(DIR *pdir);
long telldir(DIR *pdir);
void seekdir(DIR *pdir, long loc);
void rewinddir(DIR *pdir);
int closedir(DIR *pdir);
int readdir_r(DIR *pdir, struct dirent *entry, struct dirent **out_dirent);
int scandir(const char *dirname, struct dirent ***out_dirlist,
            int (*select_func)(const struct dirent *),
            int (*cmp_func)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **d1, const struct dirent **d2);

#ifdef __cplusplus
}
#endif
