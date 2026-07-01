/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef RAR_H
#define RAR_H

#include "zip.h"



int rar_open(const char *rar_path, ArchiveInfo *info);
int rar_list_files(ArchiveInfo *info);
int rar_extract_all(const char *dest, ArchiveInfo *info, int *progress);
int rar_extract_file(const char *dest, int file_index, ArchiveInfo *info);
void rar_cancel(ArchiveInfo *info);
void rar_set_password(ArchiveInfo *info, const char *password);
void rar_close(ArchiveInfo *info);

#endif
