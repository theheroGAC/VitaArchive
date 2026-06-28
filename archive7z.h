#ifndef ARCHIVE7Z_H
#define ARCHIVE7Z_H

#include "zip.h"

// 7z support using LZMA SDK
// Note: Requires LZMA SDK library to be linked

int archive7z_open(const char *archive_path, ArchiveInfo *info);
int archive7z_list_files(ArchiveInfo *info);
int archive7z_extract_all(const char *dest, ArchiveInfo *info, int *progress);
int archive7z_extract_file(const char *dest, int file_index, ArchiveInfo *info);
void archive7z_cancel(ArchiveInfo *info);
void archive7z_set_password(ArchiveInfo *info, const char *password);
void archive7z_close(ArchiveInfo *info);

#endif
