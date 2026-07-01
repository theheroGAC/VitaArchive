/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>

#include "psarc.h"
#include <stdint.h>

extern uint64_t worker_processed_bytes;

#define MAX_PATH_LENGTH 1024
#define TRANSFER_SIZE (256 * 1024)

#define SCE_FIOS_FH_SIZE 80
#define SCE_FIOS_DH_SIZE 80
#define SCE_FIOS_OP_SIZE 168
#define SCE_FIOS_CHUNK_SIZE 64

#define SCE_FIOS_ALIGN_UP(val, align) (((val) + ((align) - 1)) & ~((align) - 1))
#define SCE_FIOS_STORAGE_SIZE(num, size) (((num) * (size)) + SCE_FIOS_ALIGN_UP(SCE_FIOS_ALIGN_UP((num), 8) / 8, 8))

#define SCE_FIOS_DH_STORAGE_SIZE(numDHs, pathMax) SCE_FIOS_STORAGE_SIZE(numDHs, SCE_FIOS_DH_SIZE + pathMax)
#define SCE_FIOS_FH_STORAGE_SIZE(numFHs, pathMax) SCE_FIOS_STORAGE_SIZE(numFHs, SCE_FIOS_FH_SIZE + pathMax)
#define SCE_FIOS_OP_STORAGE_SIZE(numOps, pathMax) SCE_FIOS_STORAGE_SIZE(numOps, SCE_FIOS_OP_SIZE + pathMax)
#define SCE_FIOS_CHUNK_STORAGE_SIZE(numChunks) SCE_FIOS_STORAGE_SIZE(numChunks, SCE_FIOS_CHUNK_SIZE)

#define SCE_FIOS_BUFFER_INITIALIZER  { 0, 0 }
#define SCE_FIOS_PSARC_DEARCHIVER_CONTEXT_INITIALIZER { sizeof(SceFiosPsarcDearchiverContext), 0, 0, 0, {0, 0, 0} }
#define SCE_FIOS_PARAMS_INITIALIZER { 0, sizeof(SceFiosParams), 0, 0, 2, 1, 0, 0, 256 * 1024, 2, 0, 0, 0, 0, 0, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, NULL, NULL, NULL, { 66, 189, 66 }, { 0x40000, 0, 0x40000}, { 8 * 1024, 16 * 1024, 8 * 1024}}

typedef int32_t SceFiosFH;
typedef int32_t SceFiosDH;
typedef uint64_t SceFiosDate;
typedef int64_t SceFiosOffset;
typedef int64_t SceFiosSize;

typedef struct SceFiosPsarcDearchiverContext {
    size_t sizeOfContext;
    size_t workBufferSize;
    void *pWorkBuffer;
    intptr_t flags;
    intptr_t reserved[3];
} SceFiosPsarcDearchiverContext;

typedef struct SceFiosBuffer {
    void *pPtr;
    size_t length;
} SceFiosBuffer;

typedef struct SceFiosParams {
    uint32_t initialized : 1;
    uint32_t paramsSize : 15;
    uint32_t pathMax : 16;
    uint32_t profiling;
    uint32_t ioThreadCount;
    uint32_t threadsPerScheduler;
    uint32_t extraFlag1 : 1;
    uint32_t extraFlags : 31;
    uint32_t maxChunk;
    uint8_t maxDecompressorThreadCount;
    uint8_t reserved1;   
    uint8_t reserved2;
    uint8_t reserved3;
    intptr_t reserved4;
    intptr_t reserved5;
    SceFiosBuffer opStorage;
    SceFiosBuffer fhStorage;
    SceFiosBuffer dhStorage;
    SceFiosBuffer chunkStorage;
    void *pVprintf;
    void *pMemcpy;
    void *pProfileCallback;
    int threadPriority[3];
    int threadAffinity[3];
    int threadStackSize[3];
} SceFiosParams;

typedef struct SceFiosDirEntry {
    SceFiosOffset fileSize;
    uint32_t statFlags;
    uint16_t nameLength;
    uint16_t fullPathLength;
    uint16_t offsetToName;
    uint16_t reserved[3];
    char fullPath[1024];
} SceFiosDirEntry;

typedef struct SceFiosStat {
    SceFiosOffset fileSize;
    SceFiosDate accessDate;
    SceFiosDate modificationDate;
    SceFiosDate creationDate;
    uint32_t statFlags;
    uint32_t reserved;
    int64_t uid;
    int64_t gid;
    int64_t dev;
    int64_t ino;
    int64_t mode;
} SceFiosStat;


int sceFiosInitialize(const SceFiosParams *params);
void sceFiosTerminate();
SceFiosSize sceFiosArchiveGetMountBufferSizeSync(const void *attr, const char *path, void *params);
int sceFiosArchiveMountSync(const void *attr, SceFiosFH *fh, const char *path, const char *mount_point, SceFiosBuffer mount_buffer, void *params);
int sceFiosArchiveUnmountSync(const void *attr, SceFiosFH fh);
int sceFiosStatSync(const void *attr, const char *path, SceFiosStat *stat);
int sceFiosFHOpenSync(const void *attr, SceFiosFH *fh, const char *path, const void *params);
SceFiosSize sceFiosFHReadSync(const void *attr, SceFiosFH fh, void *data, SceFiosSize size);
int sceFiosFHCloseSync(const void *attr, SceFiosFH fh);
int sceFiosDHOpenSync(const void *attr, SceFiosDH *dh, const char *path, SceFiosBuffer buf);
int sceFiosDHReadSync(const void *attr, SceFiosDH dh, SceFiosDirEntry *dir);
int sceFiosDHCloseSync(const void *attr, SceFiosDH dh);
void sceFiosIOFilterPsarcDearchiver();
int sceFiosIOFilterAdd(int index, void (* callback)(), void *context);
int sceFiosIOFilterRemove(int index);

static int64_t g_OpStorage[SCE_FIOS_OP_STORAGE_SIZE(64, MAX_PATH_LENGTH) / sizeof(int64_t) + 1];
static int64_t g_ChunkStorage[SCE_FIOS_CHUNK_STORAGE_SIZE(1024) / sizeof(int64_t) + 1];
static int64_t g_FHStorage[SCE_FIOS_FH_STORAGE_SIZE(32, MAX_PATH_LENGTH) / sizeof(int64_t) + 1];
static int64_t g_DHStorage[SCE_FIOS_DH_STORAGE_SIZE(32, MAX_PATH_LENGTH) / sizeof(int64_t) + 1];

static SceFiosPsarcDearchiverContext g_DearchiverContext = SCE_FIOS_PSARC_DEARCHIVER_CONTEXT_INITIALIZER;
static char g_DearchiverWorkBuffer[0x30000] __attribute__((aligned(64)));
static int g_ArchiveIndex = 0;
static SceFiosBuffer g_MountBuffer = SCE_FIOS_BUFFER_INITIALIZER;
static SceFiosFH g_ArchiveFH = -1;

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

static void psarc_populate_files(const char *path, ArchiveInfo *info) {
    SceFiosDH dh = -1;
    SceFiosBuffer buf = {NULL, 0};
    if (sceFiosDHOpenSync(NULL, &dh, path, buf) >= 0) {
        int res = 0;
        do {
            SceFiosDirEntry dir;
            memset(&dir, 0, sizeof(SceFiosDirEntry));
            res = sceFiosDHReadSync(NULL, dh, &dir);
            if (res >= 0) {
                const char *rel_path = dir.fullPath + strlen(info->archive_path);
                if (rel_path[0] == '/') rel_path++;
                
                if (strlen(rel_path) > 0) {
                    if (info->file_count < MAX_ARCHIVE_FILES) {
                        ArchiveFile *file = &info->files[info->file_count];
                        strncpy(file->filename, rel_path, sizeof(file->filename) - 1);
                        file->filename[sizeof(file->filename) - 1] = '\0';
                        file->is_directory = (dir.statFlags & 0x1) ? 1 : 0;
                        file->uncompressed_size = dir.fileSize;
                        file->compressed_size = dir.fileSize;
                        
                        info->total_size += file->uncompressed_size;
                        info->total_compressed_size += file->compressed_size;
                        info->file_count++;
                    }
                }

                if (dir.statFlags & 0x1) {
                    psarc_populate_files(dir.fullPath, info);
                }
            }
        } while (res >= 0);
        sceFiosDHCloseSync(NULL, dh);
    }
}

int psarc_open(const char *psarc_path, ArchiveInfo *info) {
    memset(info, 0, sizeof(ArchiveInfo));
    strncpy(info->archive_path, psarc_path, sizeof(info->archive_path) - 1);
    
    SceFiosParams params = SCE_FIOS_PARAMS_INITIALIZER;
    params.opStorage.pPtr = g_OpStorage;
    params.opStorage.length = sizeof(g_OpStorage);
    params.chunkStorage.pPtr = g_ChunkStorage;
    params.chunkStorage.length = sizeof(g_ChunkStorage);
    params.fhStorage.pPtr = g_FHStorage;
    params.fhStorage.length = sizeof(g_FHStorage);
    params.dhStorage.pPtr = g_DHStorage;
    params.dhStorage.length = sizeof(g_DHStorage);  
    params.pathMax = MAX_PATH_LENGTH;
    
    int res = sceFiosInitialize(&params);
    if (res < 0 && res != (int)0x80010016) {
        return -1;
    }
    
    g_DearchiverContext.workBufferSize = sizeof(g_DearchiverWorkBuffer);
    g_DearchiverContext.pWorkBuffer = g_DearchiverWorkBuffer;
    sceFiosIOFilterAdd(g_ArchiveIndex, sceFiosIOFilterPsarcDearchiver, &g_DearchiverContext);
    
    SceFiosSize buf_size = sceFiosArchiveGetMountBufferSizeSync(NULL, psarc_path, NULL);
    if (buf_size < 0) {
        sceFiosIOFilterRemove(g_ArchiveIndex);
        sceFiosTerminate();
        return -1;
    }
    
    g_MountBuffer.length = (size_t)buf_size;
    g_MountBuffer.pPtr = malloc(g_MountBuffer.length);
    if (!g_MountBuffer.pPtr) {
        sceFiosIOFilterRemove(g_ArchiveIndex);
        sceFiosTerminate();
        return -1;
    }
    
    res = sceFiosArchiveMountSync(NULL, &g_ArchiveFH, psarc_path, psarc_path, g_MountBuffer, NULL);
    if (res < 0) {
        free(g_MountBuffer.pPtr);
        g_MountBuffer.pPtr = NULL;
        sceFiosIOFilterRemove(g_ArchiveIndex);
        sceFiosTerminate();
        return -1;
    }
    
    info->is_open = 1;
    info->cancel_flag = 0;
    
    psarc_populate_files(psarc_path, info);
    
    return info->file_count;
}

int psarc_extract_file(const char *dest, int file_index, ArchiveInfo *info) {
    if (!info || file_index < 0 || file_index >= info->file_count) {
        return -1;
    }
    
    ArchiveFile *file = &info->files[file_index];
    if (file->is_directory) {
        char dir_path[1024];
        size_t dest_len = strlen(dest);
        const char *filename = file->filename;
        if (filename[0] == '/') {
            filename++;
        }
        if (dest_len > 0 && dest[dest_len - 1] == '/') {
            snprintf(dir_path, sizeof(dir_path), "%s%s", dest, filename);
        } else {
            snprintf(dir_path, sizeof(dir_path), "%s/%s", dest, filename);
        }
        sceIoMkdir(dir_path, 0777);
        return 0;
    }
    
    char src_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/%s", info->archive_path, file->filename);
    
    char dst_path[1024];
    size_t dest_len = strlen(dest);
    const char *filename = file->filename;
    if (filename[0] == '/') {
        filename++;
    }
    if (dest_len > 0 && dest[dest_len - 1] == '/') {
        snprintf(dst_path, sizeof(dst_path), "%s%s", dest, filename);
    } else {
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dest, filename);
    }
    
    create_directory_path(dst_path);
    
    SceFiosFH fdsrc = -1;
    int res = sceFiosFHOpenSync(NULL, &fdsrc, src_path, NULL);
    if (res < 0) {
        return res;
    }
    
    SceUID fddst = sceIoOpen(dst_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fddst < 0) {
        sceFiosFHCloseSync(NULL, fdsrc);
        return fddst;
    }
    
    void *buf = malloc(TRANSFER_SIZE);
    if (!buf) {
        sceIoClose(fddst);
        sceFiosFHCloseSync(NULL, fdsrc);
        return -1;
    }
    
    while (1) {
        if (info->cancel_flag) {
            break;
        }
        
        SceFiosSize read = sceFiosFHReadSync(NULL, fdsrc, buf, TRANSFER_SIZE);
        if (read < 0) {
            free(buf);
            sceIoClose(fddst);
            sceFiosFHCloseSync(NULL, fdsrc);
            sceIoRemove(dst_path);
            return (int)read;
        }
        
        if (read == 0) {
            break;
        }
        
        int written = sceIoWrite(fddst, buf, (unsigned int)read);
        if (written < 0) {
            free(buf);
            sceIoClose(fddst);
            sceFiosFHCloseSync(NULL, fdsrc);
            sceIoRemove(dst_path);
            return written;
        }
        worker_processed_bytes += read;
    }
    
    free(buf);
    sceIoClose(fddst);
    sceFiosFHCloseSync(NULL, fdsrc);
    
    if (info->cancel_flag) {
        sceIoRemove(dst_path);
        return -1;
    }
    
    return 0;
}

int psarc_extract_all(const char *dest, ArchiveInfo *info, int *progress) {
    if (!info || !info->is_open) {
        return -1;
    }
    
    for (int i = 0; i < info->file_count; i++) {
        if (info->cancel_flag) {
            return -1;
        }
        
        int res = psarc_extract_file(dest, i, info);
        if (res < 0) {
            return res;
        }
        
        if (progress) {
            *progress = ((i + 1) * 100) / info->file_count;
        }
    }
    
    return 0;
}

void psarc_cancel(ArchiveInfo *info) {
    if (info) {
        info->cancel_flag = 1;
    }
}

void psarc_close(ArchiveInfo *info) {
    if (g_ArchiveFH != -1) {
        sceFiosArchiveUnmountSync(NULL, g_ArchiveFH);
        g_ArchiveFH = -1;
    }
    if (g_MountBuffer.pPtr) {
        free(g_MountBuffer.pPtr);
        g_MountBuffer.pPtr = NULL;
        g_MountBuffer.length = 0;
    }
    sceFiosIOFilterRemove(g_ArchiveIndex);
    sceFiosTerminate();
    
    if (info) {
        info->is_open = 0;
    }
}

int is_psarc_file(const char *path) {
    if (!path) return 0;
    const char *ext = strrchr(path, '.');
    if (ext) {
        return strcasecmp(ext, ".psarc") == 0;
    }
    return 0;
}
