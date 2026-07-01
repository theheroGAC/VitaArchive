/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/devctl.h>
#include <psp2/appmgr.h>
#include "globals.h"
#include "clipboard.h"

int get_partition_free_space(const char *path, uint64_t *free_space, uint64_t *total_space) {
    char dev_name[16] = {0};
    const char *colon = strchr(path, ':');
    if (colon) {
        size_t len = colon - path + 1;
        if (len < sizeof(dev_name)) {
            strncpy(dev_name, path, len);
            dev_name[len] = '\0';
        } else {
            strcpy(dev_name, "ux0:");
        }
    } else {
        strcpy(dev_name, "ux0:");
    }
    
    
    if (strcmp(dev_name, "app0:") == 0) {
        strcpy(dev_name, "ux0:");
    }
    
    
    typedef struct {
        int64_t max_size;
        int64_t free_size;
        uint32_t cluster_size;
        void *unk;
    } DevInfo;
    
    DevInfo info;
    memset(&info, 0, sizeof(info));
    int res = sceIoDevctl(dev_name, 0x3001, NULL, 0, &info, sizeof(info));
    if (res >= 0) {
        if (free_space) *free_space = (uint64_t)info.free_size;
        if (total_space) *total_space = (uint64_t)info.max_size;
        return 0;
    }
    
    
    uint64_t max_size = 0, free_size = 0;
    res = sceAppMgrGetDevInfo(dev_name, &max_size, &free_size);
    if (res == 0) {
        if (free_space) *free_space = free_size;
        if (total_space) *total_space = max_size;
        return 0;
    }
    
    return -1;
}

int copy_file_contents(const char *src, const char *dest) {
    SceUID fd_in = sceIoOpen(src, SCE_O_RDONLY, 0);
    if (fd_in < 0) return fd_in;
    
    SceUID fd_out = sceIoOpen(dest, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd_out < 0) {
        sceIoClose(fd_in);
        return fd_out;
    }
    
    char *buf = malloc(16384);
    if (!buf) {
        sceIoClose(fd_in);
        sceIoClose(fd_out);
        return -1;
    }
    
    int bytes_read;
    int res = 0;
    while ((bytes_read = sceIoRead(fd_in, buf, 16384)) > 0) {
        int bytes_written = sceIoWrite(fd_out, buf, bytes_read);
        if (bytes_written != bytes_read) {
            res = -2;
            break;
        }
        worker_processed_bytes += bytes_read;
    }
    
    free(buf);
    sceIoClose(fd_in);
    sceIoClose(fd_out);
    return res;
}

int copy_or_move_path_recursive(const char *src, const char *dest, int is_move) {
    SceIoStat stat;
    if (sceIoGetstat(src, &stat) < 0) return -1;
    
    if (SCE_S_ISDIR(stat.st_mode)) {
        sceIoMkdir(dest, 0777);
        
        SceUID dfd = sceIoDopen(src);
        if (dfd >= 0) {
            SceIoDirent entry;
            while (sceIoDread(dfd, &entry) > 0) {
                if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) continue;
                
                char next_src[512], next_dest[512];
                snprintf(next_src, sizeof(next_src), "%s/%s", src, entry.d_name);
                snprintf(next_dest, sizeof(next_dest), "%s/%s", dest, entry.d_name);
                
                copy_or_move_path_recursive(next_src, next_dest, is_move);
            }
            sceIoDclose(dfd);
        }
        
        if (is_move) {
            sceIoRmdir(src);
        }
    } else {
        int res = copy_file_contents(src, dest);
        if (res == 0 && is_move) {
            sceIoRemove(src);
        }
    }
    return 0;
}

uint64_t get_path_total_size(const char *path) {
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) < 0) return 0;
    if (SCE_S_ISDIR(stat.st_mode)) {
        uint64_t total = 0;
        SceUID dfd = sceIoDopen(path);
        if (dfd >= 0) {
            SceIoDirent entry;
            while (sceIoDread(dfd, &entry) > 0) {
                if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) continue;
                char next_path[512];
                snprintf(next_path, sizeof(next_path), "%s/%s", path, entry.d_name);
                total += get_path_total_size(next_path);
            }
            sceIoDclose(dfd);
        }
        return total;
    }
    return stat.st_size;
}
