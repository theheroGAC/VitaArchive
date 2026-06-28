#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include "rar.h"
#include "zip.h"

int sceIoMkdir(const char *path, int mode);

static struct archive *rar_archive = NULL;
static ArchiveInfo *current_rar_info = NULL;

int rar_open(const char *rar_path, ArchiveInfo *info) {
    memset(info, 0, sizeof(ArchiveInfo));
    current_rar_info = info;
    strncpy(info->archive_path, rar_path, sizeof(info->archive_path) - 1);
    info->archive_path[sizeof(info->archive_path) - 1] = '\0';
    
    rar_archive = archive_read_new();
    archive_read_support_filter_all(rar_archive);
    archive_read_support_format_all(rar_archive);
    
    if (archive_read_open_filename(rar_archive, rar_path, 10240) != ARCHIVE_OK) {
        archive_read_free(rar_archive);
        rar_archive = NULL;
        return -1;
    }
    
    info->is_open = 1;
    info->cancel_flag = 0;
    return rar_list_files(info);
}

int rar_list_files(ArchiveInfo *info) {
    struct archive_entry *entry;
    int count = 0;
    
    if (rar_archive) {
        archive_read_free(rar_archive);
        rar_archive = archive_read_new();
        archive_read_support_filter_all(rar_archive);
        archive_read_support_format_all(rar_archive);
        archive_read_open_filename(rar_archive, info->archive_path, 10240);
    }
    
    while (archive_read_next_header(rar_archive, &entry) == ARCHIVE_OK) {
        if (count < MAX_ARCHIVE_FILES) {
            strncpy(info->files[count].filename, archive_entry_pathname(entry), 255);
            info->files[count].is_directory = archive_entry_filetype(entry) == AE_IFDIR;
            info->files[count].uncompressed_size = archive_entry_size(entry);
            info->files[count].compressed_size = archive_entry_size(entry);
            
            info->total_size += info->files[count].uncompressed_size;
            info->total_compressed_size += info->files[count].compressed_size;
            count++;
        }
    }
    
    info->file_count = count;
    return count;
}

static void create_directory_path(const char *path) {
    char temp[512];
    strncpy(temp, path, sizeof(temp));
    
    char *p = temp;
    while ((p = strchr(p, '/'))) {
        *p = '\0';
        sceIoMkdir(temp, 0777);
        *p = '/';
        p++;
    }
}

int rar_extract_all(const char *dest, ArchiveInfo *info, int *progress) {
    struct archive_entry *entry;
    int count = 0;
    char out_path[512];
    
    if (rar_archive) {
        archive_read_free(rar_archive);
    }
    rar_archive = archive_read_new();
    archive_read_support_filter_all(rar_archive);
    archive_read_support_format_all(rar_archive);
    archive_read_open_filename(rar_archive, info->archive_path, 10240);
    
    while (archive_read_next_header(rar_archive, &entry) == ARCHIVE_OK) {
        if (info->cancel_flag) {
            archive_read_free(rar_archive);
            rar_archive = NULL;
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
                
                while (archive_read_data_block(rar_archive, &buff, &size, &offset) == ARCHIVE_OK) {
                    sceIoWrite(fd, buff, size);
                }
                sceIoClose(fd);
            }
        }
        
        count++;
        if (progress && info->file_count > 0) {
            *progress = (count * 100) / info->file_count;
        }
    }
    
    return 0;
}

int rar_extract_file(const char *dest, int file_index, ArchiveInfo *info) {
    return rar_extract_all(dest, info, NULL);
}

void rar_cancel(ArchiveInfo *info) {
    if (info) info->cancel_flag = 1;
}

void rar_set_password(ArchiveInfo *info, const char *password) {
    if (info && password && rar_archive) {
        archive_read_add_passphrase(rar_archive, password);
        strncpy(info->password, password, 127);
        info->password[127] = '\0';
    }
}

void rar_close(ArchiveInfo *info) {
    if (rar_archive) {
        archive_read_free(rar_archive);
        rar_archive = NULL;
    }
    if (info) {
        info->is_open = 0;
        current_rar_info = NULL;
    }
}
