/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stddef.h>


typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

void MD5_Init(MD5_CTX *context);
void MD5_Update(MD5_CTX *context, const uint8_t *input, uint32_t inputLen);
void MD5_Final(uint8_t digest[16], MD5_CTX *context);


typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} SHA256_CTX;

void SHA256_Init(SHA256_CTX *context);
void SHA256_Update(SHA256_CTX *context, const uint8_t *data, size_t len);
void SHA256_Final(uint8_t digest[32], SHA256_CTX *context);


int calculate_file_hashes(const char *filepath, char *md5_out, char *sha256_out, volatile int *cancel_flag, volatile int *progress);

#endif 
