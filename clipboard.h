/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <stdint.h>

int get_partition_free_space(const char *path, uint64_t *free_space, uint64_t *total_space);
uint64_t get_path_total_size(const char *path);
int copy_file_contents(const char *src, const char *dest);
int copy_or_move_path_recursive(const char *src, const char *dest, int is_move);

#endif
