// yaml_util.h - Friendly wrapper API over libyaml for asset extraction
#ifndef YAML_UTIL_H
#define YAML_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Opaque types (implemented in yaml_util.c)
typedef struct YamlDoc YamlDoc;
typedef struct YamlNode YamlNode;

// ============================================================================
// Document Loading/Freeing
// ============================================================================

// Load YAML document from file
// Returns NULL on error (file not found, parse error, etc.)
YamlDoc* Yaml_LoadFile(const char *path);

// Load YAML document from memory buffer
// Returns NULL on error (parse error, etc.)
YamlDoc* Yaml_LoadString(const uint8_t *data, size_t size);

// Free YAML document and all associated memory
void Yaml_Free(YamlDoc *doc);

// ============================================================================
// Document Navigation
// ============================================================================

// Get root node of document
YamlNode* Yaml_GetRoot(YamlDoc *doc);

// Get mapping value by key (for YAML objects/dictionaries)
// Returns NULL if key not found or node is not a mapping
YamlNode* Yaml_GetMapping(YamlNode *node, const char *key);

// Get sequence element by index (for YAML arrays/lists)
// Returns NULL if index out of bounds or node is not a sequence
YamlNode* Yaml_GetSequence(YamlNode *node, int index);

// Get number of elements in a sequence
// Returns 0 if node is not a sequence
int Yaml_GetSequenceLength(YamlNode *node);

// Check if mapping has a key
bool Yaml_HasKey(YamlNode *node, const char *key);

// ============================================================================
// Value Extraction (with defaults)
// ============================================================================

// Get string value
// If key doesn't exist or is not a scalar, returns default_value
const char* Yaml_GetString(YamlNode *node, const char *key, const char *default_value);

// Get integer value
// If key doesn't exist or is not a valid integer, returns default_value
int Yaml_GetInt(YamlNode *node, const char *key, int default_value);

// Get boolean value (accepts: true/false, yes/no, 1/0)
// If key doesn't exist or is not a valid boolean, returns default_value
bool Yaml_GetBool(YamlNode *node, const char *key, bool default_value);

// ============================================================================
// Direct Value Access (for sequence elements)
// ============================================================================

// Get string value from a scalar node
// Returns NULL if node is not a scalar
const char* Yaml_AsString(YamlNode *node);

// Get integer value from a scalar node
// Returns 0 if node is not a valid integer
int Yaml_AsInt(YamlNode *node);

// ============================================================================
// Error Handling
// ============================================================================

// Get last error message (valid until next Yaml_* call)
const char* Yaml_GetLastError(void);

#endif // YAML_UTIL_H
