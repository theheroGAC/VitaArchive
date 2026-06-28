#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <archive.h>
#include <archive_entry.h>

#include "zip.h"

int sceIoMkdir(const char *path, int mode);

static struct archive *zip_archive = NULL;

static int ends_with_ignore_case(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;

    return strcasecmp(str + str_len - suffix_len, suffix) == 0;
}

static int open_archive_reader(const char *path) {
    if (zip_archive) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
    }

    zip_archive = archive_read_new();
    archive_read_support_filter_all(zip_archive);
    archive_read_support_format_all(zip_archive);

    if (archive_read_open_filename(zip_archive, path, 10240) != ARCHIVE_OK) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
        return -1;
    }

    return 0;
}

static void create_directory_path(const char *path) {
    char temp[512];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *p = temp;
    while ((p = strchr(p, '/'))) {
        *p = '\0';
        sceIoMkdir(temp, 0777);
        *p = '/';
        p++;
    }
}

static void skip_entry_data(struct archive *archive) {
    const void *buff;
    size_t size;
    int64_t offset;

    while (archive_read_data_block(archive, &buff, &size, &offset) == ARCHIVE_OK) {
    }
}

int zip_open(const char *zip_path, ArchiveInfo *info) {
    memset(info, 0, sizeof(ArchiveInfo));
    strncpy(info->archive_path, zip_path, sizeof(info->archive_path) - 1);
    info->archive_path[sizeof(info->archive_path) - 1] = '\0';
    info->is_open = 1;
    info->cancel_flag = 0;

    if (open_archive_reader(zip_path) < 0) {
        info->is_open = 0;
        return -1;
    }

    struct archive_entry *entry;
    int count = 0;

    while (archive_read_next_header(zip_archive, &entry) == ARCHIVE_OK) {
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

int zip_extract_all(const char *dest, ArchiveInfo *info, int *progress) {
    if (!info || !info->is_open) {
        return -1;
    }

    if (open_archive_reader(info->archive_path) < 0) {
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

                while (archive_read_data_block(zip_archive, &buff, &size, &offset) == ARCHIVE_OK) {
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

    archive_read_free(zip_archive);
    zip_archive = NULL;

    return 0;
}

int zip_extract_file_to(const char *archive_path, int file_index, const char *dest_path, ArchiveInfo *info) {
    struct archive *archive = archive_read_new();
    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);

    if (archive_read_open_filename(archive, archive_path, 10240) != ARCHIVE_OK) {
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
                return -3;
            }

            const void *buff;
            size_t size;
            int64_t offset;

            while (archive_read_data_block(archive, &buff, &size, &offset) == ARCHIVE_OK) {
                sceIoWrite(fd, buff, size);
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

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s%s", dest, info->files[file_index].filename);
    return zip_extract_file_to(info->archive_path, file_index, out_path, info);
}

int zip_create(const char *zip_path, const char **files, int file_count) {
    struct archive *zip_writer;
    struct archive_entry *entry;
    char buffer[8192];
    int fd;

    zip_writer = archive_write_new();
    archive_write_set_format_zip(zip_writer);
    archive_write_open_filename(zip_writer, zip_path);

    for (int i = 0; i < file_count; i++) {
        fd = sceIoOpen(files[i], SCE_O_RDONLY, 0);
        if (fd >= 0) {
            entry = archive_entry_new();
            archive_entry_set_pathname(entry, files[i]);
            archive_entry_set_size(entry, sceIoLseek(fd, 0, SCE_SEEK_END));
            sceIoLseek(fd, 0, SCE_SEEK_SET);
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, 0644);
            archive_write_header(zip_writer, entry);

            int bytes_read;
            while ((bytes_read = sceIoRead(fd, buffer, sizeof(buffer))) > 0) {
                archive_write_data(zip_writer, buffer, bytes_read);
            }

            archive_entry_free(entry);
            sceIoClose(fd);
        }
    }

    archive_write_close(zip_writer);
    archive_write_free(zip_writer);

    return 0;
}

void zip_cancel(ArchiveInfo *info) {
    if (info) {
        info->cancel_flag = 1;
    }
}

void zip_set_password(ArchiveInfo *info, const char *password) {
    if (info && password && zip_archive) {
        archive_read_add_passphrase(zip_archive, password);
        strncpy(info->password, password, 127);
        info->password[127] = '\0';
    }
}

void zip_close(ArchiveInfo *info) {
    if (zip_archive) {
        archive_read_free(zip_archive);
        zip_archive = NULL;
    }
    if (info) {
        info->is_open = 0;
        info->archive_path[0] = '\0';
    }
}

int is_zip_file(const char *path) {
    return ends_with_ignore_case(path, ".zip");
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
    return ends_with_ignore_case(path, ".gz") ||
           ends_with_ignore_case(path, ".tgz") ||
           ends_with_ignore_case(path, ".tar.gz");
}

int is_bzip2_file(const char *path) {
    return ends_with_ignore_case(path, ".bz2") ||
           ends_with_ignore_case(path, ".tbz") ||
           ends_with_ignore_case(path, ".tbz2") ||
           ends_with_ignore_case(path, ".tar.bz2");
}
