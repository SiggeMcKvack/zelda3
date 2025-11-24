// sha256.h - Public domain SHA-256 implementation
// Based on Brad Conte's crypto-algorithms
// https://github.com/B-Con/crypto-algorithms

#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t data[64];
  uint32_t datalen;
  uint64_t bitlen;
  uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[32]);

// Convenience function: hash data in one shot
void sha256(const uint8_t *data, size_t len, uint8_t hash[32]);

#endif // SHA256_H
