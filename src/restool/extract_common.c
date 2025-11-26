// extract_common.c - Shared helper functions for asset extraction
#include "extract.h"
#include "asset_reader.h"
#include "../logging.h"

// Load YAML from embedded assets or filesystem
YamlDoc* LoadAssetYaml(const char *path) {
  size_t size;
  const uint8_t *embedded = AssetReader_GetEmbedded(path, &size);
  if (embedded) {
    return Yaml_LoadString(embedded, size);
  }
  // Fall back to filesystem
  return Yaml_LoadFile(path);
}

// Load binary/text data from embedded assets or filesystem
// Returns allocated buffer that caller must free with AssetReader_Free()
uint8_t* LoadAssetData(const char *path, size_t *out_size) {
  return AssetReader_Load(path, out_size);
}
