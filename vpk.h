#ifndef VPK_H
#define VPK_H

#include "zip.h"

#define VPK_PKG_DIR  "ux0:/data/VitaArchive/pkg"
#define VPK_TEMP_DIR "ux0:/data/VitaArchive/temp"

int is_vpk_file(const char *path);
int vpk_install_from_zip(ArchiveInfo *zip_info, int vpk_index, int *progress);
void vpk_cleanup_dirs(void);

#endif
