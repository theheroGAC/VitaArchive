/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef PSARC_H
#define PSARC_H

#include "zip.h" 

int psarc_open(const char *psarc_path, ArchiveInfo *info);
int psarc_extract_all(const char *dest, ArchiveInfo *info, int *progress);
int psarc_extract_file(const char *dest, int file_index, ArchiveInfo *info);
void psarc_cancel(ArchiveInfo *info);
void psarc_close(ArchiveInfo *info);
int is_psarc_file(const char *path);

#endif
