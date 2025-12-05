# SHA-256 Implementation

**Source:** Based on Brad Conte's crypto-algorithms (https://github.com/B-Con/crypto-algorithms)
**Version:** Public domain implementation
**License:** Public Domain
**Date Added:** 2025-11-23

## Purpose
Provides SHA-256 hashing for:
1. ROM validation (verify correct ROM file via SHA-256 hash)
2. Asset file checksums (`zelda3_assets.dat` integrity verification)

Used by the asset extraction tool (`zelda3-restool`) to ensure data integrity.

## Files
- `sha256.h` - Header with SHA256_CTX structure and function declarations
- `sha256.c` - Implementation (~150 LOC)

## Usage
```c
#include "sha256.h"

// Method 1: One-shot hashing
uint8_t hash[32];
sha256(data, data_len, hash);

// Method 2: Incremental hashing (for large files)
SHA256_CTX ctx;
sha256_init(&ctx);
sha256_update(&ctx, chunk1, chunk1_len);
sha256_update(&ctx, chunk2, chunk2_len);
sha256_final(&ctx, hash);
```

## Modifications
Minor cleanup and formatting from original source. Core algorithm unchanged.

## Updating
This is a stable, well-tested implementation. Updates unlikely to be needed unless security issues are discovered (none known as of 2025).

## Why Vendored?
- Small, self-contained implementation (~150 LOC)
- Public domain (no licensing concerns)
- No need for heavy crypto libraries (OpenSSL, mbedtls) for simple hashing
- Keeps tool dependency-free
