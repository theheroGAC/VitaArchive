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
#include <psp2/io/stat.h>
#include <archive.h>
#include <archive_entry.h>

#include "zip.h"

int sceIoMkdir(const char *path, int mode);
extern uint64_t worker_processed_bytes;

static struct archive *zip_archive = NULL;

static int ends_with_ignore_case(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcasecmp(str + str_len - suffix_len, suffix) == 0;
}

int is_zip_file(const char *path) {
    return ends_with_ignore_case(path, ".zip") || ends_with_ignore_case(path, ".vpk");
}
int is_rar_file(const char *path) {
    return ends_with_ignore_case(path, ".rar");
}
int is_7z_file(const char *path) {
    return ends_with_ignore_case(path, ".7z");
}
int is_tar_file(const char *path) {
    return ends_with_ignore_case(path, ".tar");
}
int is_gzip_file(const char *path) {
    return ends_with_ignore_case(path, ".gz");
}
int is_bzip2_file(const char *path) {
    return ends_with_ignore_case(path, ".bz2");
}

static int is_archive_encrypted_error(struct archive *a, int res) {
    if (res != ARCHIVE_OK && res != ARCHIVE_EOF) {
        const char *err_str = archive_error_string(a);
        if (err_str && (strstr(err_str, "passphrase") || strstr(err_str, "password") || strstr(err_str, "decrypt") || strstr(err_str, "encrypted"))) {
            return 1;
        }
    }
    return 0;
}

static int open_archive_reader(const char *path, const char *password) {
    if (zip_archive) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
    }
    zip_archive = archive_read_new();
    archive_read_support_filter_all(zip_archive);
    archive_read_support_format_all(zip_archive);
    
    if (password && password[0] != '\0') {
        archive_read_add_passphrase(zip_archive, password);
    }
    
    if (open_archive_read(zip_archive, path) != ARCHIVE_OK) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
        return -1;
    }
    return 0;
}

static void skip_entry_data(struct archive *a) {
    const void *buff;
    size_t size;
    int64_t offset;
    while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {}
}

int zip_open(const char *zip_path, ArchiveInfo *info) {
    char password_backup[128] = {0};
    if (info) {
        strncpy(password_backup, info->password, sizeof(password_backup) - 1);
    }

    memset(info, 0, sizeof(ArchiveInfo));
    strncpy(info->archive_path, zip_path, sizeof(info->archive_path) - 1);
    strncpy(info->password, password_backup, sizeof(info->password) - 1);
    info->is_open = 1;
    info->cancel_flag = 0;

    if (open_archive_reader(zip_path, info->password) < 0) {
        info->is_open = 0;
        return -1;
    }

    struct archive_entry *entry;
    int count = 0;
    int res;

    while ((res = archive_read_next_header(zip_archive, &entry)) == ARCHIVE_OK) {
        if (archive_entry_is_encrypted(entry)) {
            info->is_encrypted = 1;
            if (info->password[0] == '\0') {
                archive_read_free(zip_archive);
                zip_archive = NULL;
                return -5; 
            }
        }

        if (count < MAX_ARCHIVE_FILES) {
            strncpy(info->files[count].filename, archive_entry_pathname(entry), 255);
            info->files[count].filename[255] = '\0';
            info->files[count].is_directory = archive_entry_filetype(entry) == AE_IFDIR;
            info->files[count].uncompressed_size = archive_entry_size(entry);
            info->files[count].compressed_size = archive_entry_size(entry);

            info->total_size += info->files[count].uncompressed_size;
            info->total_compressed_size += info->files[count].compressed_size;
            count++;
        }
        skip_entry_data(zip_archive);
    }

    if (is_archive_encrypted_error(zip_archive, res)) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
        return -5; 
    }

    info->file_count = count;
    archive_read_free(zip_archive);
    zip_archive = NULL;

    return count;
}

int zip_list_files(ArchiveInfo *info) {
    if (!info || !info->is_open || info->archive_path[0] == '\0') {
        return -1;
    }
    return zip_open(info->archive_path, info);
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

int zip_extract_all(const char *dest, ArchiveInfo *info, int *progress) {
    if (!info || !info->is_open) {
        return -1;
    }

    if (open_archive_reader(info->archive_path, info->password) < 0) {
        return -1;
    }

    struct archive_entry *entry;
    int count = 0;
    char out_path[512];

    while (archive_read_next_header(zip_archive, &entry) == ARCHIVE_OK) {
        if (info->cancel_flag) {
            archive_read_free(zip_archive);
            zip_archive = NULL;
            return -1;
        }

        const char *entry_path = archive_entry_pathname(entry);
        if (entry_path[0] == '/') {
            entry_path++;
        }
        size_t dest_len = strlen(dest);
        if (dest_len > 0 && dest[dest_len - 1] == '/') {
            snprintf(out_path, sizeof(out_path), "%s%s", dest, entry_path);
        } else {
            snprintf(out_path, sizeof(out_path), "%s/%s", dest, entry_path);
        }

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            sceIoMkdir(out_path, 0777);
        } else {
            create_directory_path(out_path);

            SceUID fd = sceIoOpen(out_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
            if (fd >= 0) {
                const void *buff;
                size_t size;
                int64_t offset;

                while (archive_read_data_block(zip_archive, &buff, &size, &offset) == ARCHIVE_OK) {
                    sceIoWrite(fd, buff, size);
                    worker_processed_bytes += size;
                }
                sceIoClose(fd);
            } else {
                archive_read_free(zip_archive);
                zip_archive = NULL;
                return fd;
            }
        }

        count++;
        if (progress && info->file_count > 0) {
            *progress = (count * 100) / info->file_count;
        }
    }

    archive_read_free(zip_archive);
    zip_archive = NULL;

    return 0;
}

int zip_extract_file_to(const char *archive_path, int file_index, const char *dest_path, ArchiveInfo *info) {
    struct archive *archive = archive_read_new();
    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);

    if (info && info->password[0] != '\0') {
        archive_read_add_passphrase(archive, info->password);
    }

    if (open_archive_read(archive, archive_path) != ARCHIVE_OK) {
        archive_read_free(archive);
        return -1;
    }

    struct archive_entry *entry;
    int idx = 0;
    int result = -1;

    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        if (info && info->cancel_flag) {
            archive_read_free(archive);
            return -1;
        }

        if (idx == file_index) {
            if (archive_entry_filetype(entry) == AE_IFDIR) {
                archive_read_free(archive);
                return -2;
            }

            create_directory_path(dest_path);

            SceUID fd = sceIoOpen(dest_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
            if (fd < 0) {
                archive_read_free(archive);
                return fd;
            }

            const void *buff;
            size_t size;
            int64_t offset;

            while (archive_read_data_block(archive, &buff, &size, &offset) == ARCHIVE_OK) {
                sceIoWrite(fd, buff, size);
                worker_processed_bytes += size;
            }

            sceIoClose(fd);
            result = 0;
            break;
        }

        skip_entry_data(archive);
        idx++;
    }

    archive_read_free(archive);
    return result;
}

int zip_extract_file(const char *dest, int file_index, ArchiveInfo *info) {
    if (!info || file_index < 0 || file_index >= info->file_count) {
        return -1;
    }

    char out_path[1024];
    size_t dest_len = strlen(dest);
    const char *filename = info->files[file_index].filename;
    if (filename[0] == '/') {
        filename++;
    }
    if (dest_len > 0 && dest[dest_len - 1] == '/') {
        snprintf(out_path, sizeof(out_path), "%s%s", dest, filename);
    } else {
        snprintf(out_path, sizeof(out_path), "%s/%s", dest, filename);
    }
    return zip_extract_file_to(info->archive_path, file_index, out_path, info);
}

int archive_create_custom_format(const char *archive_path, const char **files, int file_count, int format_type) {
    extern int compress_level;
    struct archive *a;
    struct archive_entry *entry;
    char buff[16384];
    int len;
    SceUID fd;

    a = archive_write_new();
    
    if (format_type == 0) {
        archive_write_set_format_zip(a);
        if (compress_level == 0) {
            archive_write_set_options(a, "zip:compression=store");
        } else if (compress_level == 2) {
            archive_write_set_options(a, "zip:compression-level=9");
        } else {
            archive_write_set_options(a, "zip:compression-level=6");
        }
    } else if (format_type == 1) {
        archive_write_set_format_7zip(a);
        if (compress_level == 0) {
            archive_write_set_options(a, "7zip:compression-level=0");
        } else if (compress_level == 2) {
            archive_write_set_options(a, "7zip:compression-level=9");
        } else {
            archive_write_set_options(a, "7zip:compression-level=6");
        }
    } else if (format_type == 2) {
        archive_write_set_format_ustar(a);
    } else if (format_type == 3) {
        archive_write_set_format_ustar(a);
        archive_write_add_filter_gzip(a);
        if (compress_level == 0) {
            archive_write_set_options(a, "gzip:compression-level=1");
        } else if (compress_level == 2) {
            archive_write_set_options(a, "gzip:compression-level=9");
        }
    } else if (format_type == 4) {
        archive_write_set_format_ustar(a);
        archive_write_add_filter_bzip2(a);
        if (compress_level == 0) {
            archive_write_set_options(a, "bzip2:compression-level=1");
        } else if (compress_level == 2) {
            archive_write_set_options(a, "bzip2:compression-level=9");
        }
    } else {
        archive_write_free(a);
        return -2;
    }

    if (archive_write_open_filename(a, archive_path) != ARCHIVE_OK) {
        archive_write_free(a);
        return -1;
    }

    for (int i = 0; i < file_count; i++) {
        const char *filepath = files[i];
        
        const char *filename = filepath;
        const char *slash = strrchr(filepath, '/');
        if (slash) {
            filename = slash + 1;
        }
        
        entry = archive_entry_new();
        archive_entry_set_pathname(entry, filename);
        
        SceIoStat stat;
        if (sceIoGetstat(filepath, &stat) < 0) {
            archive_entry_free(entry);
            continue;
        }
        
        archive_entry_set_size(entry, stat.st_size);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        
        archive_write_header(a, entry);
        
        fd = sceIoOpen(filepath, SCE_O_RDONLY, 0);
        if (fd >= 0) {
            while ((len = sceIoRead(fd, buff, sizeof(buff))) > 0) {
                archive_write_data(a, buff, len);
                worker_processed_bytes += len;
            }
            sceIoClose(fd);
        }
        
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
    return 0;
}

int zip_create(const char *zip_path, const char **files, int file_count) {
    return archive_create_custom_format(zip_path, files, file_count, 0);
}

void zip_cancel(ArchiveInfo *info) {
    if (info) {
        info->cancel_flag = 1;
    }
}

void zip_set_password(ArchiveInfo *info, const char *password) {
    if (info) {
        strncpy(info->password, password, sizeof(info->password) - 1);
    }
}

void zip_close(ArchiveInfo *info) {
    if (info) {
        info->is_open = 0;
    }
    if (zip_archive) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
    }
}

int archive_test_integrity(ArchiveInfo *info, int *progress) {
    if (!info || info->archive_path[0] == '\0') {
        return -1;
    }
    
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
    int count = 0;
    int result = 0;
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        if (info->cancel_flag) {
            result = -1;
            break;
        }
        
        if (archive_entry_filetype(entry) != AE_IFDIR) {
            const void *buff;
            size_t size;
            int64_t offset;
            
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {

            }
        }
        
        count++;
        if (progress && info->file_count > 0) {
            *progress = (count * 100) / info->file_count;
        }
    }
    
    archive_read_free(a);
    return result;
}

struct SplitReaderData {
    char base_path[1024];
    int current_part;
    SceUID fd;
    char read_buf[16384];
};

static int split_open(struct archive *a, void *client_data) {
    struct SplitReaderData *data = (struct SplitReaderData *)client_data;
    char part_path[2048];
    snprintf(part_path, sizeof(part_path), "%s%03d", data->base_path, data->current_part);
    
    data->fd = sceIoOpen(part_path, SCE_O_RDONLY, 0);
    if (data->fd < 0) return ARCHIVE_FATAL;
    return ARCHIVE_OK;
}

static la_ssize_t split_read(struct archive *a, void *client_data, const void **buff) {
    struct SplitReaderData *data = (struct SplitReaderData *)client_data;
    *buff = data->read_buf;
    
    int bytes = sceIoRead(data->fd, data->read_buf, sizeof(data->read_buf));
    if (bytes < 0) return ARCHIVE_FATAL;
    
    if (bytes == 0) {
        sceIoClose(data->fd);
        data->current_part++;
        
        char part_path[2048];
        snprintf(part_path, sizeof(part_path), "%s%03d", data->base_path, data->current_part);
        
        data->fd = sceIoOpen(part_path, SCE_O_RDONLY, 0);
        if (data->fd < 0) {
            data->fd = -1;
            return 0;
        }
        
        bytes = sceIoRead(data->fd, data->read_buf, sizeof(data->read_buf));
        if (bytes < 0) return ARCHIVE_FATAL;
    }
    
    return bytes;
}

static int split_close(struct archive *a, void *client_data) {
    struct SplitReaderData *data = (struct SplitReaderData *)client_data;
    if (data) {
        if (data->fd >= 0) {
            sceIoClose(data->fd);
        }
        free(data);
    }
    return ARCHIVE_OK;
}

int open_archive_read(struct archive *a, const char *path) {
    const char *ext = strrchr(path, '.');
    int is_split = 0;
    if (ext && strlen(ext) == 4) {
        is_split = 1;
        for (int i = 1; i <= 3; i++) {
            if (ext[i] < '0' || ext[i] > '9') {
                is_split = 0;
                break;
            }
        }
    }
    
    if (is_split) {
        struct SplitReaderData *data = malloc(sizeof(struct SplitReaderData));
        if (!data) return ARCHIVE_FATAL;
        
        size_t len = ext - path + 1;
        if (len >= 1024) len = 1023;
        strncpy(data->base_path, path, len);
        data->base_path[len] = '\0';
        
        data->current_part = atoi(ext + 1);
        data->fd = -1;
        
        return archive_read_open(a, data, split_open, split_read, split_close);
    } else {
        return archive_read_open_filename(a, path, 10240);
    }
}
