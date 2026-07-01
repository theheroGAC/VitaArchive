/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#include "hash.h"
#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>



#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5_Transform(uint32_t state[4], const uint8_t block[64]);

static uint8_t PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac) { \
    (a) += F ((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) { \
    (a) += G ((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) { \
    (a) += H ((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) { \
    (a) += I ((b), (c), (d)) + (x) + (uint32_t)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
  }

void MD5_Init(MD5_CTX *context) {
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

void MD5_Update(MD5_CTX *context, const uint8_t *input, uint32_t inputLen) {
    uint32_t i, index, partLen;

    index = (uint32_t)((context->count[0] >> 3) & 0x3F);

    if ((context->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) {
        context->count[1]++;
    }
    context->count[1] += ((uint32_t)inputLen >> 29);

    partLen = 64 - index;

    if (inputLen >= partLen) {
        memcpy((void *)&context->buffer[index], (const void *)input, partLen);
        MD5_Transform(context->state, context->buffer);

        for (i = partLen; i + 63 < inputLen; i += 64) {
            MD5_Transform(context->state, &input[i]);
        }

        index = 0;
    } else {
        i = 0;
    }

    memcpy((void *)&context->buffer[index], (const void *)&input[i], inputLen - i);
}

void MD5_Final(uint8_t digest[16], MD5_CTX *context) {
    uint8_t bits[8];
    uint32_t index, padLen;

    bits[0] = (uint8_t)(context->count[0] & 0xFF);
    bits[1] = (uint8_t)((context->count[0] >> 8) & 0xFF);
    bits[2] = (uint8_t)((context->count[0] >> 16) & 0xFF);
    bits[3] = (uint8_t)((context->count[0] >> 24) & 0xFF);
    bits[4] = (uint8_t)(context->count[1] & 0xFF);
    bits[5] = (uint8_t)((context->count[1] >> 8) & 0xFF);
    bits[6] = (uint8_t)((context->count[1] >> 16) & 0xFF);
    bits[7] = (uint8_t)((context->count[1] >> 24) & 0xFF);

    index = (uint32_t)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    MD5_Update(context, PADDING, padLen);

    MD5_Update(context, bits, 8);

    for (int i = 0; i < 4; i++) {
        digest[i*4]   = (uint8_t)(context->state[i] & 0xFF);
        digest[i*4+1] = (uint8_t)((context->state[i] >> 8) & 0xFF);
        digest[i*4+2] = (uint8_t)((context->state[i] >> 16) & 0xFF);
        digest[i*4+3] = (uint8_t)((context->state[i] >> 24) & 0xFF);
    }

    memset((void *)context, 0, sizeof(*context));
}

static void MD5_Transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    for (int i = 0, j = 0; j < 64; i++, j += 4) {
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j+1]) << 8) |
               (((uint32_t)block[j+2]) << 16) | (((uint32_t)block[j+3]) << 24);
    }

    
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); 
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); 
    FF (c, d, a, b, x[ 2], S13, 0x242070db); 
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); 
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); 
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); 
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); 
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); 
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); 
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); 
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); 
    FF (b, c, d, a, x[11], S14, 0x895cd7be); 
    FF (a, b, c, d, x[12], S11, 0x6b901122); 
    FF (d, a, b, c, x[13], S12, 0xfd987193); 
    FF (c, d, a, b, x[14], S13, 0xa679438e); 
    FF (b, c, d, a, x[15], S14, 0x49b40821); 

    
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); 
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); 
    GG (c, d, a, b, x[11], S23, 0x265e5a51); 
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); 
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); 
    GG (d, a, b, c, x[10], S22,  0x2441453); 
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); 
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); 
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); 
    GG (d, a, b, c, x[14], S22, 0xc33707d6); 
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); 
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); 
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); 
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); 
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); 
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); 

    
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); 
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); 
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); 
    HH (b, c, d, a, x[14], S34, 0xfde5380c); 
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); 
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); 
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); 
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); 
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); 
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); 
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); 
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); 
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); 
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); 
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); 
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); 

    
    II (a, b, c, d, x[ 0], S41, 0xf4292244); 
    II (d, a, b, c, x[ 7], S42, 0x432aff97); 
    II (c, d, a, b, x[14], S43, 0xab9423a7); 
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); 
    II (a, b, c, d, x[12], S41, 0x655b59c3); 
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); 
    II (c, d, a, b, x[10], S43, 0xffeff47d); 
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); 
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); 
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); 
    II (c, d, a, b, x[ 6], S43, 0xa3014314); 
    II (b, c, d, a, x[13], S44, 0x4e0811a1); 
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); 
    II (d, a, b, c, x[11], S42, 0xbd3af235); 
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); 
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); 

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    memset((void *)x, 0, sizeof(x));
}



#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void SHA256_Transform(SHA256_CTX *ctx, const uint8_t *data) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];

    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
               ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
    }

    for (int i = 16; i < 64; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (int i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void SHA256_Init(SHA256_CTX *context) {
    context->count = 0;
    context->state[0] = 0x6a09e667;
    context->state[1] = 0xbb67ae85;
    context->state[2] = 0x3c6ef372;
    context->state[3] = 0xa54ff53a;
    context->state[4] = 0x510e527f;
    context->state[5] = 0x9b05688c;
    context->state[6] = 0x1f83d9ab;
    context->state[7] = 0x5be0cd19;
}

void SHA256_Update(SHA256_CTX *context, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        context->buffer[context->count & 63] = data[i];
        context->count++;
        if ((context->count & 63) == 0) {
            SHA256_Transform(context, context->buffer);
        }
    }
}

void SHA256_Final(uint8_t digest[32], SHA256_CTX *context) {
    uint64_t i = context->count;
    uint8_t pad_len = ((context->count & 63) < 56) ? (56 - (context->count & 63)) : (120 - (context->count & 63));

    uint8_t pad[120];
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    SHA256_Update(context, pad, pad_len);

    uint8_t bits[8];
    bits[0] = (uint8_t)(i >> 53);
    bits[1] = (uint8_t)(i >> 45);
    bits[2] = (uint8_t)(i >> 37);
    bits[3] = (uint8_t)(i >> 29);
    bits[4] = (uint8_t)(i >> 21);
    bits[5] = (uint8_t)(i >> 13);
    bits[6] = (uint8_t)(i >> 5);
    bits[7] = (uint8_t)(i << 3);
    SHA256_Update(context, bits, 8);

    for (int j = 0; j < 8; ++j) {
        digest[j * 4] = (uint8_t)(context->state[j] >> 24);
        digest[j * 4 + 1] = (uint8_t)(context->state[j] >> 16);
        digest[j * 4 + 2] = (uint8_t)(context->state[j] >> 8);
        digest[j * 4 + 3] = (uint8_t)(context->state[j]);
    }

    memset((void *)context, 0, sizeof(*context));
}



int calculate_file_hashes(const char *filepath, char *md5_out, char *sha256_out, volatile int *cancel_flag, volatile int *progress) {
    SceUID fd = sceIoOpen(filepath, SCE_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

    SceIoStat stat;
    if (sceIoGetstat(filepath, &stat) < 0) {
        sceIoClose(fd);
        return -1;
    }
    uint64_t total_size = stat.st_size;

    MD5_CTX md5_ctx;
    SHA256_CTX sha256_ctx;
    MD5_Init(&md5_ctx);
    SHA256_Init(&sha256_ctx);

    #define HASH_BUF_SIZE (64 * 1024)
    static uint8_t buf[HASH_BUF_SIZE];
    uint64_t processed_size = 0;
    int bytes_read;

    if (progress) {
        *progress = 0;
    }

    while ((bytes_read = sceIoRead(fd, buf, HASH_BUF_SIZE)) > 0) {
        if (cancel_flag && *cancel_flag) {
            sceIoClose(fd);
            return -2; 
        }

        MD5_Update(&md5_ctx, buf, bytes_read);
        SHA256_Update(&sha256_ctx, buf, bytes_read);

        processed_size += bytes_read;
        if (progress && total_size > 0) {
            *progress = (int)((processed_size * 100) / total_size);
        }
    }

    sceIoClose(fd);

    uint8_t md5_digest[16];
    uint8_t sha256_digest[32];
    MD5_Final(md5_digest, &md5_ctx);
    SHA256_Final(sha256_digest, &sha256_ctx);

    
    char *p = md5_out;
    for (int i = 0; i < 16; i++) {
        sprintf(p, "%02x", md5_digest[i]);
        p += 2;
    }
    *p = '\0';

    p = sha256_out;
    for (int i = 0; i < 32; i++) {
        sprintf(p, "%02x", sha256_digest[i]);
        p += 2;
    }
    *p = '\0';

    if (progress) {
        *progress = 100;
    }

    return 0;
}
