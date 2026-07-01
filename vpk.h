/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef VPK_H
#define VPK_H

#include "zip.h"

#define VPK_PKG_DIR  "ux0:/ptmp/pkg"
#define VPK_TEMP_DIR "ux0:/ptmp/temp"

int is_vpk_file(const char *path);
int archive_contains_homebrew(const ArchiveInfo *info);
int vpk_install_from_zip(ArchiveInfo *zip_info, int vpk_index, int *progress);
int vpk_install_homebrew_from_archive(ArchiveInfo *archive_info, int *progress);
void vpk_cleanup_dirs(void);
int remove_path_recursive(const char *path);
int archive_can_smart_install(const ArchiveInfo *info);

#endif
