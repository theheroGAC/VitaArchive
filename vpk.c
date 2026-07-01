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
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>
#include <psp2/kernel/threadmgr.h>

#include "vpk.h"
#include "zip.h"
#include "head_bin.h"

#define ntohl __builtin_bswap32
#define SFO_MAGIC 0x46535000

typedef struct SfoHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t keyofs;
    uint32_t valofs;
    uint32_t count;
} __attribute__((packed)) SfoHeader;

typedef struct SfoEntry {
    uint16_t nameofs;
    uint8_t  alignment;
    uint8_t  type;
    uint32_t valsize;
    uint32_t totalsize;
    uint32_t dataofs;
} __attribute__((packed)) SfoEntry;


typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define blk0(i) (block->l[i] = (rol(block->l[i], 24) & 0xFF00FF00) | (rol(block->l[i], 8) & 0x00FF00FF))
#define blk(i) (block->l[i & 15] = rol(block->l[(i + 13) & 15] ^ block->l[(i + 8) & 15] ^ block->l[(i + 2) & 15] ^ block->l[i & 15], 1))

#define r0(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + block->l[i] + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define r1(v,w,x,y,z,i) z += ((w & (x ^ y)) ^ y) + blk(i) + 0x5A827999 + rol(v, 5); w = rol(w, 30);
#define r2(v,w,x,y,z,i) z += (w ^ x ^ y) + blk(i) + 0x6ED9EBA1 + rol(v, 5); w = rol(w, 30);
#define r3(v,w,x,y,z,i) z += (((w | x) & y) | (w & x)) + blk(i) + 0x8F1BBCDC + rol(v, 5); w = rol(w, 30);
#define r4(v,w,x,y,z,i) z += (w ^ x ^ y) + blk(i) + 0xCA62C1D6 + rol(v, 5); w = rol(w, 30);

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    typedef union {
        uint8_t c[64];
        uint32_t l[16];
    } CHAR64LONG16;
    CHAR64LONG16 block[1];
    memcpy(block, buffer, 64);
    
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    
    for (int i = 0; i < 16; i++) {
        blk0(i);
    }
    
    r0(a, b, c, d, e, 0); r0(e, a, b, c, d, 1); r0(d, e, a, b, c, 2); r0(c, d, e, a, b, 3);
    r0(b, c, d, e, a, 4); r0(a, b, c, d, e, 5); r0(e, a, b, c, d, 6); r0(d, e, a, b, c, 7);
    r0(c, d, e, a, b, 8); r0(b, c, d, e, a, 9); r0(a, b, c, d, e, 10); r0(e, a, b, c, d, 11);
    r0(d, e, a, b, c, 12); r0(c, d, e, a, b, 13); r0(b, c, d, e, a, 14); r0(a, b, c, d, e, 15);
    r1(e, a, b, c, d, 16); r1(d, e, a, b, c, 17); r1(c, d, e, a, b, 18); r1(b, c, d, e, a, 19);
    r2(a, b, c, d, e, 20); r2(e, a, b, c, d, 21); r2(d, e, a, b, c, 22); r2(c, d, e, a, b, 23);
    r2(b, c, d, e, a, 24); r2(a, b, c, d, e, 25); r2(e, a, b, c, d, 26); r2(d, e, a, b, c, 27);
    r2(c, d, e, a, b, 28); r2(b, c, d, e, a, 29); r2(a, b, c, d, e, 30); r2(e, a, b, c, d, 31);
    r2(d, e, a, b, c, 32); r2(c, d, e, a, b, 33); r2(b, c, d, e, a, 34); r2(a, b, c, d, e, 35);
    r2(e, a, b, c, d, 36); r2(d, e, a, b, c, 37); r2(c, d, e, a, b, 38); r2(b, c, d, e, a, 39);
    r3(a, b, c, d, e, 40); r3(e, a, b, c, d, 41); r3(d, e, a, b, c, 42); r3(c, d, e, a, b, 43);
    r3(b, c, d, e, a, 44); r3(a, b, c, d, e, 45); r3(e, a, b, c, d, 46); r3(d, e, a, b, c, 47);
    r3(c, d, e, a, b, 48); r3(b, c, d, e, a, 49); r3(a, b, c, d, e, 50); r3(e, a, b, c, d, 51);
    r3(d, e, a, b, c, 52); r3(c, d, e, a, b, 53); r3(b, c, d, e, a, 54); r3(a, b, c, d, e, 55);
    r3(e, a, b, c, d, 56); r3(d, e, a, b, c, 57); r3(c, d, e, a, b, 58); r3(b, c, d, e, a, 59);
    r4(a, b, c, d, e, 60); r4(e, a, b, c, d, 61); r4(d, e, a, b, c, 62); r4(c, d, e, a, b, 63);
    r4(b, c, d, e, a, 64); r4(a, b, c, d, e, 65); r4(e, a, b, c, d, 66); r4(d, e, a, b, c, 67);
    r4(c, d, e, a, b, 68); r4(b, c, d, e, a, 69); r4(a, b, c, d, e, 70); r4(e, a, b, c, d, 71);
    r4(d, e, a, b, c, 72); r4(c, d, e, a, b, 73); r4(b, c, d, e, a, 74); r4(a, b, c, d, e, 75);
    r4(e, a, b, c, d, 76); r4(d, e, a, b, c, 77); r4(c, d, e, a, b, 78); r4(b, c, d, e, a, 79);
    
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(SHA1_CTX *context) {
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}

static void sha1_update(SHA1_CTX *context, const uint8_t *data, uint32_t len) {
    uint32_t i, j;
    j = context->count[0];
    if ((context->count[0] += len << 3) < j) {
        context->count[1]++;
    }
    context->count[1] += (len >> 29);
    j = (j >> 3) & 63;
    if ((j + len) > 63) {
        memcpy(&context->buffer[j], data, (i = 64 - j));
        sha1_transform(context->state, context->buffer);
        for (; i + 63 < len; i += 64) {
            sha1_transform(context->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&context->buffer[j], &data[i], len - i);
}

static void sha1_final(SHA1_CTX *context, uint8_t digest[20]) {
    unsigned i;
    uint8_t finalcount[8];
    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((context->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 255);
    }
    uint8_t c = 0200;
    sha1_update(context, &c, 1);
    while ((context->count[0] & 504) != 448) {
        uint8_t zero = 0;
        sha1_update(context, &zero, 1);
    }
    sha1_update(context, finalcount, 8);
    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}


static void fpkg_hmac(const uint8_t* data, unsigned int len, uint8_t hmac[16]) {
    SHA1_CTX ctx;
    uint8_t sha1[20];
    uint8_t buf[64];

    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, sha1);

    memset(buf, 0, 64);
    memcpy(&buf[0], &sha1[4], 8);
    memcpy(&buf[8], &sha1[4], 8);
    memcpy(&buf[16], &sha1[12], 4);
    buf[20] = sha1[16];
    buf[21] = sha1[1];
    buf[22] = sha1[2];
    buf[23] = sha1[3];
    memcpy(&buf[24], &buf[16], 8);

    sha1_init(&ctx);
    sha1_update(&ctx, buf, 64);
    sha1_final(&ctx, sha1);
    memcpy(hmac, sha1, 16);
}

int is_vpk_file(const char *path) {
    const char *ext = strrchr(path, '.');
    return ext && strcasecmp(ext, ".vpk") == 0;
}

static const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int remove_path_recursive(const char *path) {
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

static int dir_exists(const char *path) {
    SceIoStat stat;
    return sceIoGetstat(path, &stat) >= 0 && SCE_S_ISDIR(stat.st_mode);
}

static int directory_has_vita_app(const char *path) {
    char eboot_path[512];
    char param_path[512];

    snprintf(eboot_path, sizeof(eboot_path), "%s/eboot.bin", path);
    snprintf(param_path, sizeof(param_path), "%s/sce_sys/param.sfo", path);

    return file_exists(eboot_path) && file_exists(param_path);
}

static int find_vita_app_root_recursive(const char *path, char *out_path, size_t out_size) {
    if (directory_has_vita_app(path)) {
        strncpy(out_path, path, out_size - 1);
        out_path[out_size - 1] = '\0';
        return 0;
    }

    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) {
        return -1;
    }

    SceIoDirent ent;
    char child[512];
    int found = -1;

    while (sceIoDread(dfd, &ent) > 0) {
        if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) {
            continue;
        }

        snprintf(child, sizeof(child), "%s/%s", path, ent.d_name);
        if (dir_exists(child) && find_vita_app_root_recursive(child, out_path, out_size) == 0) {
            found = 0;
            break;
        }
    }

    sceIoDclose(dfd);
    return found;
}

static int get_sfo_string(const char *sfo_path, const char *name, char *out_val, int max_len) {
    SceUID fd = sceIoOpen(sfo_path, SCE_O_RDONLY, 0);
    if (fd < 0) return fd;
    
    int size = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    
    void *buf = malloc(size);
    if (!buf) {
        sceIoClose(fd);
        return -1;
    }
    
    if (sceIoRead(fd, buf, size) != size) {
        free(buf);
        sceIoClose(fd);
        return -2;
    }
    sceIoClose(fd);
    
    SfoHeader *header = (SfoHeader*)buf;
    SfoEntry *entries = (SfoEntry*)((uintptr_t)buf + sizeof(SfoHeader));
    
    if (header->magic != SFO_MAGIC) {
        free(buf);
        return -3;
    }
    
    for (int i = 0; i < header->count; i++) {
        const char *entry_key = (const char *)buf + header->keyofs + entries[i].nameofs;
        if (strcmp(entry_key, name) == 0) {
            memset(out_val, 0, max_len);
            strncpy(out_val, (const char *)buf + header->valofs + entries[i].dataofs, max_len - 1);
            out_val[max_len - 1] = '\0';
            free(buf);
            return 0;
        }
    }
    
    free(buf);
    return -4;
}

static int make_head(const char *path) {
    char tmp_path[1024];
    uint8_t hmac[16];
    uint32_t off;
    uint32_t len;
    uint32_t out;


    char titleid[16] = {0};
    snprintf(tmp_path, sizeof(tmp_path), "%s/sce_sys/param.sfo", path);
    if (get_sfo_string(tmp_path, "TITLE_ID", titleid, sizeof(titleid)) < 0) {
        return -1;
    }


    char contentid[48] = {0};
    get_sfo_string(tmp_path, "CONTENT_ID", contentid, sizeof(contentid));


    uint8_t *head_bin = malloc(tpl_head_bin_len);
    if (!head_bin) return -1;
    memcpy(head_bin, tpl_head_bin, tpl_head_bin_len);


    char full_title_id[48];
    snprintf(full_title_id, sizeof(full_title_id), "EP9000-%s_00-0000000000000000", titleid);
    strncpy((char*)&head_bin[0x30], strlen(contentid) > 0 ? contentid : full_title_id, 47);
    head_bin[0x30 + 47] = '\0';


    len = ntohl(*(uint32_t*)&head_bin[0xD0]);
    fpkg_hmac(&head_bin[0], len, hmac);
    memcpy(&head_bin[len], hmac, 16);


    off = ntohl(*(uint32_t*)&head_bin[0x8]);
    len = ntohl(*(uint32_t*)&head_bin[0x10]);
    out = ntohl(*(uint32_t*)&head_bin[0xD4]);
    fpkg_hmac(&head_bin[off], len - 64, hmac);
    memcpy(&head_bin[out], hmac, 16);


    len = ntohl(*(uint32_t*)&head_bin[0xE8]);
    fpkg_hmac(&head_bin[0], len, hmac);
    memcpy(&head_bin[len], hmac, 16);


    snprintf(tmp_path, sizeof(tmp_path), "%s/sce_sys/package", path);
    sceIoMkdir(tmp_path, 0777);


    snprintf(tmp_path, sizeof(tmp_path), "%s/sce_sys/package/head.bin", path);
    SceUID fd = sceIoOpen(tmp_path, SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 0777);
    if (fd < 0) {
        free(head_bin);
        return fd;
    }
    int res = sceIoWrite(fd, head_bin, tpl_head_bin_len);
    sceIoClose(fd);

    free(head_bin);
    return res;
}

static int promote_pkg(const char *path) {
    int res = make_head(path);
    if (res < 0) {
        return res;
    }

    res = scePromoterUtilityPromotePkgWithRif(path, 1);
    if (res < 0) {
        res = scePromoterUtilityPromotePkg(path, 1);
    }
    if (res < 0) {
        return res;
    }


    int state = 0;
    do {
        sceKernelDelayThread(100000);
        int state_res = scePromoterUtilityGetState(&state);
        if (state_res < 0) {
            break;
        }
    } while (state != 0);

    int op_res = 0;
    int result_res = scePromoterUtilityGetResult(&op_res);
    if (result_res >= 0) {
        return op_res;
    }

    return res;
}

static int install_extracted_vpk(const char *vpk_path, int *progress) {
    ArchiveInfo *vpk_info = malloc(sizeof(ArchiveInfo));
    if (!vpk_info) {
        return -1;
    }

    if (zip_open(vpk_path, vpk_info) < 0) {
        free(vpk_info);
        return -1;
    }

    remove_path_recursive(VPK_PKG_DIR);
    ensure_parent_dirs(VPK_PKG_DIR);
    sceIoMkdir(VPK_PKG_DIR, 0777);

    if (zip_extract_all(VPK_PKG_DIR "/", vpk_info, progress) < 0) {
        zip_close(vpk_info);
        free(vpk_info);
        remove_path_recursive(VPK_PKG_DIR);
        return -2;
    }
    zip_close(vpk_info);
    free(vpk_info);

    char sfo_path[512];
    snprintf(sfo_path, sizeof(sfo_path), "%s/sce_sys/param.sfo", VPK_PKG_DIR);
    if (!file_exists(sfo_path)) {
        remove_path_recursive(VPK_PKG_DIR);
        return -3;
    }
    if (!file_exists(VPK_PKG_DIR "/eboot.bin")) {
        remove_path_recursive(VPK_PKG_DIR);
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

int archive_contains_homebrew(const ArchiveInfo *info) {
    if (!info) return 0;

    for (int i = 0; i < info->file_count; i++) {
        const char *name = path_basename(info->files[i].filename);
        if (strcasecmp(name, "eboot.bin") == 0) {
            return 1;
        }
    }

    return 0;
}

int vpk_install_homebrew_from_archive(ArchiveInfo *archive_info, int *progress) {
    if (!archive_info || !archive_info->is_open || !archive_contains_homebrew(archive_info)) {
        return -1;
    }

    if (progress) {
        *progress = 0;
    }

    remove_path_recursive(VPK_PKG_DIR);
    ensure_parent_dirs(VPK_PKG_DIR);
    sceIoMkdir(VPK_PKG_DIR, 0777);

    if (zip_extract_all(VPK_PKG_DIR "/", archive_info, progress) < 0) {
        remove_path_recursive(VPK_PKG_DIR);
        return -2;
    }

    char app_root[512];
    if (find_vita_app_root_recursive(VPK_PKG_DIR, app_root, sizeof(app_root)) < 0) {
        remove_path_recursive(VPK_PKG_DIR);
        return -3;
    }

    char sfo_path[1024];
    snprintf(sfo_path, sizeof(sfo_path), "%s/sce_sys/param.sfo", app_root);
    if (!file_exists(sfo_path)) {
        remove_path_recursive(VPK_PKG_DIR);
        return -4;
    }

    if (progress) {
        *progress = 95;
    }

    int res = promote_pkg(app_root);
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

int archive_can_smart_install(const ArchiveInfo *info) {
    if (!info || !info->is_open) return 0;
    const char *ext = strrchr(info->archive_path, '.');
    int is_zip = ext && strcasecmp(ext, ".zip") == 0;
    return is_zip && archive_contains_homebrew(info);
}
