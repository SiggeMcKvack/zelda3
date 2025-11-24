// asset_compiler.c - Zelda3 asset compilation implementation

#include "asset_compiler.h"
#include "types.h"
#include "../platform.h"
#include "../logging.h"
#include "../../third_party/sha256/sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ASSET_SIGNATURE "Zelda3_v0     \n"
#define INITIAL_ASSET_CAPACITY 64

AssetBuilder* AssetBuilder_Create(void) {
  AssetBuilder *builder = (AssetBuilder*)malloc(sizeof(AssetBuilder));
  if (!builder) {
    LogError("Failed to allocate AssetBuilder");
    return NULL;
  }

  builder->assets = (Asset*)malloc(INITIAL_ASSET_CAPACITY * sizeof(Asset));
  if (!builder->assets) {
    LogError("Failed to allocate assets array");
    free(builder);
    return NULL;
  }

  builder->asset_count = 0;
  builder->asset_capacity = INITIAL_ASSET_CAPACITY;

  return builder;
}

void AssetBuilder_AddAsset(AssetBuilder *builder, const char *name, AssetType type,
                           const uint8_t *data, uint32_t size) {
  if (!builder || !name || !data) {
    LogError("Invalid arguments to AssetBuilder_AddAsset");
    return;
  }

  // Grow array if needed
  if (builder->asset_count >= builder->asset_capacity) {
    uint32_t new_capacity = builder->asset_capacity * 2;
    Asset *new_assets = (Asset*)realloc(builder->assets, new_capacity * sizeof(Asset));
    if (!new_assets) {
      LogError("Failed to grow assets array");
      return;
    }
    builder->assets = new_assets;
    builder->asset_capacity = new_capacity;
  }

  // Copy asset data
  uint8_t *data_copy = (uint8_t*)malloc(size);
  if (!data_copy) {
    LogError("Failed to allocate asset data copy");
    return;
  }
  memcpy(data_copy, data, size);

  // Store asset
  Asset *asset = &builder->assets[builder->asset_count++];
  asset->name = strdup(name);
  asset->type = type;
  asset->data = data_copy;
  asset->size = size;
}

uint8_t* AssetBuilder_PackArrays(uint8_t **arrays, uint32_t *sizes, uint32_t count, uint32_t *out_size) {
  if (!arrays || !sizes || count == 0 || !out_size) {
    LogError("Invalid arguments to AssetBuilder_PackArrays");
    return NULL;
  }

  // Calculate total data size and offsets
  uint32_t *offsets = (uint32_t*)malloc((count - 1) * sizeof(uint32_t));
  if (!offsets) {
    LogError("Failed to allocate offsets array");
    return NULL;
  }

  uint32_t total_data_size = 0;
  for (uint32_t i = 0; i < count - 1; i++) {
    total_data_size += sizes[i];
    offsets[i] = total_data_size;
  }
  total_data_size += sizes[count - 1];  // Add last array size

  // Determine format: uint16 or uint32 offsets
  bool use_uint16 = (total_data_size < 65536 && count <= 8192);
  uint32_t offset_section_size = (count - 1) * (use_uint16 ? 2 : 4);
  uint32_t count_size = 2;  // Always uint16 for count
  uint32_t packed_size = offset_section_size + total_data_size + count_size;

  // Allocate packed buffer
  uint8_t *packed = (uint8_t*)malloc(packed_size);
  if (!packed) {
    LogError("Failed to allocate packed buffer (%u bytes)", packed_size);
    free(offsets);
    return NULL;
  }

  uint8_t *ptr = packed;

  // Write offsets
  for (uint32_t i = 0; i < count - 1; i++) {
    if (use_uint16) {
      uint16_t offset16 = (uint16_t)offsets[i];
      memcpy(ptr, &offset16, 2);
      ptr += 2;
    } else {
      memcpy(ptr, &offsets[i], 4);
      ptr += 4;
    }
  }

  // Write array data
  for (uint32_t i = 0; i < count; i++) {
    memcpy(ptr, arrays[i], sizes[i]);
    ptr += sizes[i];
  }

  // Write count
  uint16_t count_value = use_uint16 ? (count - 1) : (8192 + count - 1);
  memcpy(ptr, &count_value, 2);
  ptr += 2;

  free(offsets);
  *out_size = packed_size;
  return packed;
}

bool AssetBuilder_WriteToFile(AssetBuilder *builder, const char *filepath) {
  if (!builder || !filepath) {
    LogError("Invalid arguments to AssetBuilder_WriteToFile");
    return false;
  }

  FILE *fp = fopen(filepath, "wb");
  if (!fp) {
    LogError("Failed to open output file: %s", filepath);
    return false;
  }

  // Calculate key signature (null-terminated asset names concatenated)
  size_t key_sig_size = 0;
  for (uint32_t i = 0; i < builder->asset_count; i++) {
    key_sig_size += strlen(builder->assets[i].name) + 1;  // +1 for null terminator
  }

  uint8_t *key_sig = (uint8_t*)malloc(key_sig_size);
  if (!key_sig) {
    LogError("Failed to allocate key signature buffer");
    fclose(fp);
    return false;
  }

  size_t offset = 0;
  for (uint32_t i = 0; i < builder->asset_count; i++) {
    size_t name_len = strlen(builder->assets[i].name) + 1;
    memcpy(key_sig + offset, builder->assets[i].name, name_len);
    offset += name_len;
  }

  // Calculate SHA-256 of key signature
  uint8_t key_hash[32];
  SHA256_CTX ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, key_sig, key_sig_size);
  sha256_final(&ctx, key_hash);

  // Write header
  // Signature (16 bytes)
  fwrite(ASSET_SIGNATURE, 1, 16, fp);

  // SHA-256 hash of key signature (32 bytes)
  fwrite(key_hash, 1, 32, fp);

  // Reserved (32 bytes of zeros)
  uint8_t reserved[32] = {0};
  fwrite(reserved, 1, 32, fp);

  // Number of assets (4 bytes, little-endian)
  uint32_t num_assets = builder->asset_count;
  fwrite(&num_assets, 4, 1, fp);

  // Size of key signature (4 bytes, little-endian)
  uint32_t key_sig_size_le = (uint32_t)key_sig_size;
  fwrite(&key_sig_size_le, 4, 1, fp);

  // Write asset sizes array
  for (uint32_t i = 0; i < builder->asset_count; i++) {
    uint32_t size = builder->assets[i].size;
    fwrite(&size, 4, 1, fp);
  }

  // Write key signature (asset names)
  fwrite(key_sig, 1, key_sig_size, fp);

  // Write asset data (4-byte aligned)
  for (uint32_t i = 0; i < builder->asset_count; i++) {
    // Align to 4-byte boundary
    long pos = ftell(fp);
    while (pos % 4 != 0) {
      fputc(0, fp);
      pos++;
    }

    // Write asset data
    fwrite(builder->assets[i].data, 1, builder->assets[i].size, fp);
  }

  fclose(fp);
  free(key_sig);

  LogInfo("Wrote %u assets to %s", builder->asset_count, filepath);
  return true;
}

void AssetBuilder_Free(AssetBuilder *builder) {
  if (!builder) return;

  for (uint32_t i = 0; i < builder->asset_count; i++) {
    free((void*)builder->assets[i].name);
    free(builder->assets[i].data);
  }

  free(builder->assets);
  free(builder);
}
