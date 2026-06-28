#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>

#include "vpk.h"
#include "zip.h"

int is_vpk_file(const char *path) {
    const char *ext = strrchr(path, '.');
    return ext && strcasecmp(ext, ".vpk") == 0;
}

static int remove_path_recursive(const char *path) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) < 0) {
        return 0;
    }

    if (SCE_S_ISDIR(stat.st_mode)) {
        SceUID dfd = sceIoDopen(path);
        if (dfd < 0) {
            return sceIoRmdir(path);
        }

        SceIoDirent ent;
        char child[512];
        while (sceIoDread(dfd, &ent) > 0) {
            snprintf(child, sizeof(child), "%s/%s", path, ent.d_name);
            remove_path_recursive(child);
        }
        sceIoDclose(dfd);
        return sceIoRmdir(path);
    }

    return sceIoRemove(path);
}

static int ensure_parent_dirs(const char *path) {
    char temp[512];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *slash = strchr(temp + 5, '/');
    while (slash) {
        *slash = '\0';
        sceIoMkdir(temp, 0777);
        *slash = '/';
        slash = strchr(slash + 1, '/');
    }
    return 0;
}

static int file_exists(const char *path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0 && SCE_S_ISREG(stat.st_mode);
}

static int promote_pkg(const char *path) {
    int res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (res < 0) {
        return res;
    }

    res = scePromoterUtilityInit();
    if (res < 0) {
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
        return res;
    }

    res = scePromoterUtilityPromotePkgWithRif(path, 1);
    if (res < 0) {
        res = scePromoterUtilityPromotePkg(path, 1);
    }

    int exit_res = scePromoterUtilityExit();
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);

    if (res < 0) {
        return res;
    }
    return exit_res < 0 ? exit_res : 0;
}

static int install_extracted_vpk(const char *vpk_path, int *progress) {
    ArchiveInfo vpk_info;

    if (zip_open(vpk_path, &vpk_info) < 0) {
        return -1;
    }

    remove_path_recursive(VPK_PKG_DIR);
    ensure_parent_dirs(VPK_PKG_DIR);
    sceIoMkdir(VPK_PKG_DIR, 0777);

    if (zip_extract_all(VPK_PKG_DIR "/", &vpk_info, progress) < 0) {
        zip_close(&vpk_info);
        return -2;
    }
    zip_close(&vpk_info);

    if (!file_exists(VPK_PKG_DIR "/sce_sys/param.sfo")) {
        return -3;
    }
    if (!file_exists(VPK_PKG_DIR "/eboot.bin")) {
        return -4;
    }

    if (progress) {
        *progress = 95;
    }

    int res = promote_pkg(VPK_PKG_DIR);
    remove_path_recursive(VPK_PKG_DIR);

    if (progress) {
        *progress = res == 0 ? 100 : *progress;
    }

    return res;
}

void vpk_cleanup_dirs(void) {
    remove_path_recursive(VPK_TEMP_DIR);
    remove_path_recursive(VPK_PKG_DIR);
}

int vpk_install_from_zip(ArchiveInfo *zip_info, int vpk_index, int *progress) {
    if (!zip_info || !zip_info->is_open || vpk_index < 0 || vpk_index >= zip_info->file_count) {
        return -1;
    }

    if (!is_vpk_file(zip_info->files[vpk_index].filename)) {
        return -1;
    }

    if (progress) {
        *progress = 0;
    }

    remove_path_recursive(VPK_TEMP_DIR);
    ensure_parent_dirs(VPK_TEMP_DIR);
    sceIoMkdir(VPK_TEMP_DIR, 0777);

    char vpk_temp[512];
    snprintf(vpk_temp, sizeof(vpk_temp), "%s/install.vpk", VPK_TEMP_DIR);

    if (zip_extract_file_to(zip_info->archive_path, vpk_index, vpk_temp, zip_info) < 0) {
        return -2;
    }

    if (progress) {
        *progress = 40;
    }

    int res = install_extracted_vpk(vpk_temp, progress);
    sceIoRemove(vpk_temp);

    return res;
}
