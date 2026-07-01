/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <psp2/io/fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include "archive7z.h"
#include "zip.h"

int sceIoMkdir(const char *path, int mode);
extern uint64_t worker_processed_bytes;

static struct archive *archive7z = NULL;
static ArchiveInfo *current_7z_info = NULL;

static int is_archive_encrypted_error(struct archive *a, int res) {
    if (res != ARCHIVE_OK && res != ARCHIVE_EOF) {
        const char *err_str = archive_error_string(a);
        if (err_str && (strstr(err_str, "passphrase") || strstr(err_str, "password") || strstr(err_str, "decrypt") || strstr(err_str, "encrypted"))) {
            return 1;
        }
    }
    return 0;
}

int archive7z_open(const char *archive_path, ArchiveInfo *info) {
    char password_backup[128] = {0};
    if (info) {
        strncpy(password_backup, info->password, sizeof(password_backup) - 1);
    }
    
    memset(info, 0, sizeof(ArchiveInfo));
    current_7z_info = info;
    strncpy(info->archive_path, archive_path, sizeof(info->archive_path) - 1);
    strncpy(info->password, password_backup, sizeof(info->password) - 1);
    
    info->is_open = 1;
    info->cancel_flag = 0;
    return archive7z_list_files(info);
}

int archive7z_list_files(ArchiveInfo *info) {
    struct archive_entry *entry;
    int count = 0;    
    struct archive *a;
    
    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    
    if (info->password[0] != '\0') {
        archive_read_add_passphrase(a, info->password);
    }
    
    if (open_archive_read(a, info->archive_path) != ARCHIVE_OK) {
        archive_read_free(a);
        return -1;
    }

    info->file_count = 0;
    info->total_size = 0;
    info->total_compressed_size = 0;
    memset(info->files, 0, sizeof(info->files));
    
    int res;
    while ((res = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        if (archive_entry_is_encrypted(entry)) {
            info->is_encrypted = 1;
            if (info->password[0] == '\0') {
                archive_read_free(a);
                return -5; 
            }
        }
        
        if (count < MAX_ARCHIVE_FILES) {
            strncpy(info->files[count].filename, archive_entry_pathname(entry), 255);
            info->files[count].is_directory = archive_entry_filetype(entry) == AE_IFDIR;
            info->files[count].uncompressed_size = archive_entry_size(entry);
            info->files[count].compressed_size = archive_entry_size(entry);
            
            info->total_size += info->files[count].uncompressed_size;
            info->total_compressed_size += info->files[count].compressed_size;
            count++;
        }
        const void *buff;
        size_t size;
        int64_t offset;
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {}
    }
    
    if (is_archive_encrypted_error(a, res)) {
        archive_read_free(a);
        return -5; 
    }
    
    info->file_count = count;
    archive_read_free(a);

    return count;
}

static void create_directory_path(const char *path) {
    char temp[512];
    strncpy(temp, path, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';
    
    char *p = temp;
    char *col = strchr(p, ':');
    if (col) {
        p = col + 1;
    }
    if (*p == '/') {
        p++;
    }
    
    while ((p = strchr(p, '/'))) {
        *p = '\0';
        sceIoMkdir(temp, 0777);
        *p = '/';
        p++;
    }
}

int archive7z_extract_all(const char *dest, ArchiveInfo *info, int *progress) {
    struct archive_entry *entry;
    int count = 0;
    char out_path[512];
    
    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    
    if (info->password[0] != '\0') {
        archive_read_add_passphrase(a, info->password);
    }
    
    if (open_archive_read(a, info->archive_path) != ARCHIVE_OK) {
        archive_read_free(a);
        return -1;
    }
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (info->cancel_flag) {
            archive_read_free(a);
            return -1;
        }
        
        snprintf(out_path, sizeof(out_path), "%s%s", dest, archive_entry_pathname(entry));
        
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            sceIoMkdir(out_path, 0777);
        } else {
            create_directory_path(out_path);
            
            SceUID fd = sceIoOpen(out_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
            if (fd >= 0) {
                const void *buff;
                size_t size;
                int64_t offset;
                
                while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                    sceIoWrite(fd, buff, size);
                    worker_processed_bytes += size;
                }
                sceIoClose(fd);
            }
        }
        
        count++;
        if (progress && info->file_count > 0) {
            *progress = (count * 100) / info->file_count;
        }
    }
    
    archive_read_free(a);
    return 0;
}
 
int archive7z_extract_file(const char *dest, int file_index, ArchiveInfo *info) {
    if (!info || file_index < 0 || file_index >= info->file_count) {
        return -1;
    }

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s%s", dest, info->files[file_index].filename);

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    if (info->password[0] != '\0') {
        archive_read_add_passphrase(a, info->password);
    }

    if (open_archive_read(a, info->archive_path) != ARCHIVE_OK) {
        archive_read_free(a);
        return -1;
    }

    struct archive_entry *entry;
    int idx = 0;
    int result = -1;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (info->cancel_flag) {
            result = -1;
            break;
        }

        if (idx == file_index) {
            if (archive_entry_filetype(entry) == AE_IFDIR) {
                result = -2;
                break;
            }

            create_directory_path(out_path);
            SceUID fd = sceIoOpen(out_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
            if (fd < 0) {
                result = -3;
                break;
            }

            const void *buff;
            size_t size;
            int64_t offset;

            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                sceIoWrite(fd, buff, size);
                worker_processed_bytes += size;
            }

            sceIoClose(fd);
            result = 0;
            break;
        }

        const void *buff;
        size_t size;
        int64_t offset;
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {}
        idx++;
    }

    archive_read_free(a);
    return result;
}

void archive7z_cancel(ArchiveInfo *info) {
    if (info) info->cancel_flag = 1;
}

void archive7z_close(ArchiveInfo *info) {
    if (info) info->is_open = 0;
    if (archive7z) {
        archive_read_free(archive7z);
        archive7z = NULL;
    }
}
