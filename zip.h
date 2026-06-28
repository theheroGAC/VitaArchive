#ifndef ZIP_H
#define ZIP_H

#define MAX_ARCHIVE_FILES 512

typedef struct {
    char filename[256];
    size_t compressed_size;
    size_t uncompressed_size;
    int is_directory;
} ArchiveFile;

typedef struct {
    ArchiveFile files[MAX_ARCHIVE_FILES];
    int file_count;
    size_t total_size;
    size_t total_compressed_size;
    int is_open;
    int cancel_flag;
    char password[128];
    char archive_path[512];
} ArchiveInfo;

int zip_open(const char *zip_path, ArchiveInfo *info);
int zip_list_files(ArchiveInfo *info);
int zip_extract_all(const char *dest, ArchiveInfo *info, int *progress);
int zip_extract_file(const char *dest, int file_index, ArchiveInfo *info);
int zip_extract_file_to(const char *archive_path, int file_index, const char *dest_path, ArchiveInfo *info);
int zip_create(const char *zip_path, const char **files, int file_count);
void zip_cancel(ArchiveInfo *info);
void zip_set_password(ArchiveInfo *info, const char *password);
void zip_close(ArchiveInfo *info);

int is_zip_file(const char *path);
int is_rar_file(const char *path);
int is_7z_file(const char *path);
int is_tar_file(const char *path);
int is_gzip_file(const char *path);
int is_bzip2_file(const char *path);

#endif
