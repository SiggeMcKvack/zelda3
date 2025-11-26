// yaml_util.c - YAML parsing utilities implementation
#include "yaml_util.h"
#include "../logging.h"
#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Internal Structures
// ============================================================================

// Node pool for memory management - all nodes tied to document lifetime
#define YAML_NODE_POOL_INITIAL_CAPACITY 32

typedef struct {
  YamlNode *nodes;
  int count;
  int capacity;
} YamlNodePool;

struct YamlDoc {
  yaml_document_t document;
  yaml_parser_t parser;
  FILE *file;
  YamlNodePool node_pool;  // Memory pool for YamlNode allocations
};

struct YamlNode {
  YamlDoc *doc;             // Back-pointer to owning document (for pool allocation)
  yaml_document_t *document;
  yaml_node_t *node;
};

// Thread-local error message storage
// Use portable thread-local macro
#if defined(__GNUC__) || defined(__clang__)
  #define THREAD_LOCAL __thread
#elif defined(_MSC_VER)
  #define THREAD_LOCAL __declspec(thread)
#else
  #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_yaml_error[256] = {0};

// ============================================================================
// Error Handling
// ============================================================================

static void SetError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_yaml_error, sizeof(g_yaml_error), fmt, args);
  va_end(args);
}

const char* Yaml_GetLastError(void) {
  return g_yaml_error;
}

// ============================================================================
// Node Pool Management
// ============================================================================

// Allocate a YamlNode from the document's pool
// Nodes are automatically freed when Yaml_Free() is called
static YamlNode* AllocNode(YamlDoc *doc) {
  if (!doc) return NULL;

  YamlNodePool *pool = &doc->node_pool;

  // Grow pool if needed
  if (pool->count >= pool->capacity) {
    int new_capacity = pool->capacity ? pool->capacity * 2 : YAML_NODE_POOL_INITIAL_CAPACITY;
    YamlNode *new_nodes = realloc(pool->nodes, new_capacity * sizeof(YamlNode));
    if (!new_nodes) {
      SetError("Failed to grow node pool");
      return NULL;
    }
    pool->nodes = new_nodes;
    pool->capacity = new_capacity;
  }

  return &pool->nodes[pool->count++];
}

// Free all nodes in the pool
static void FreeNodePool(YamlNodePool *pool) {
  if (pool && pool->nodes) {
    free(pool->nodes);
    pool->nodes = NULL;
    pool->count = 0;
    pool->capacity = 0;
  }
}

// ============================================================================
// Document Loading/Freeing
// ============================================================================

YamlDoc* Yaml_LoadFile(const char *path) {
  if (!path) {
    SetError("NULL path provided");
    return NULL;
  }

  YamlDoc *doc = calloc(1, sizeof(YamlDoc));
  if (!doc) {
    SetError("Failed to allocate YamlDoc");
    return NULL;
  }

  // Open file
  doc->file = fopen(path, "rb");
  if (!doc->file) {
    SetError("Failed to open file: %s", path);
    free(doc);
    return NULL;
  }

  // Initialize parser
  if (!yaml_parser_initialize(&doc->parser)) {
    SetError("Failed to initialize YAML parser");
    fclose(doc->file);
    free(doc);
    return NULL;
  }

  // Set input file
  yaml_parser_set_input_file(&doc->parser, doc->file);

  // Load document
  if (!yaml_parser_load(&doc->parser, &doc->document)) {
    SetError("Failed to parse YAML file: %s (line %zu)",
             doc->parser.problem, doc->parser.problem_mark.line);
    yaml_parser_delete(&doc->parser);
    fclose(doc->file);
    free(doc);
    return NULL;
  }

  g_yaml_error[0] = '\0';  // Clear error on success
  return doc;
}

YamlDoc* Yaml_LoadString(const uint8_t *data, size_t size) {
  if (!data || size == 0) {
    SetError("NULL or empty data provided");
    return NULL;
  }

  YamlDoc *doc = calloc(1, sizeof(YamlDoc));
  if (!doc) {
    SetError("Failed to allocate YamlDoc");
    return NULL;
  }

  doc->file = NULL;  // No file for string-based loading

  // Initialize parser
  if (!yaml_parser_initialize(&doc->parser)) {
    SetError("Failed to initialize YAML parser");
    free(doc);
    return NULL;
  }

  // Set input from string buffer
  yaml_parser_set_input_string(&doc->parser, data, size);

  // Load document
  if (!yaml_parser_load(&doc->parser, &doc->document)) {
    SetError("Failed to parse YAML data (line %zu): %s",
             doc->parser.problem_mark.line, doc->parser.problem);
    yaml_parser_delete(&doc->parser);
    free(doc);
    return NULL;
  }

  g_yaml_error[0] = '\0';  // Clear error on success
  return doc;
}

void Yaml_Free(YamlDoc *doc) {
  if (!doc) return;

  // Free the node pool (all YamlNode allocations)
  FreeNodePool(&doc->node_pool);

  yaml_document_delete(&doc->document);
  yaml_parser_delete(&doc->parser);

  if (doc->file) {
    fclose(doc->file);
  }

  free(doc);
}

// ============================================================================
// Document Navigation
// ============================================================================

YamlNode* Yaml_GetRoot(YamlDoc *doc) {
  if (!doc) {
    SetError("NULL document");
    return NULL;
  }

  yaml_node_t *root = yaml_document_get_root_node(&doc->document);
  if (!root) {
    SetError("Document has no root node");
    return NULL;
  }

  YamlNode *node = AllocNode(doc);
  if (!node) {
    return NULL;  // Error already set by AllocNode
  }

  node->doc = doc;
  node->document = &doc->document;
  node->node = root;
  return node;
}

YamlNode* Yaml_GetMapping(YamlNode *node, const char *key) {
  if (!node || !key) {
    SetError("NULL node or key");
    return NULL;
  }

  if (node->node->type != YAML_MAPPING_NODE) {
    SetError("Node is not a mapping");
    return NULL;
  }

  // Search for key in mapping
  for (yaml_node_pair_t *pair = node->node->data.mapping.pairs.start;
       pair < node->node->data.mapping.pairs.top; pair++) {

    yaml_node_t *key_node = yaml_document_get_node(node->document, pair->key);
    if (key_node && key_node->type == YAML_SCALAR_NODE) {
      const char *key_str = (const char*)key_node->data.scalar.value;
      if (strcmp(key_str, key) == 0) {
        // Found matching key, return value node
        yaml_node_t *value_node = yaml_document_get_node(node->document, pair->value);
        if (!value_node) {
          SetError("Invalid value node for key: %s", key);
          return NULL;
        }

        YamlNode *result = AllocNode(node->doc);
        if (!result) {
          return NULL;  // Error already set by AllocNode
        }

        result->doc = node->doc;
        result->document = node->document;
        result->node = value_node;
        return result;
      }
    }
  }

  SetError("Key not found: %s", key);
  return NULL;
}

YamlNode* Yaml_GetSequence(YamlNode *node, int index) {
  if (!node) {
    SetError("NULL node");
    return NULL;
  }

  if (node->node->type != YAML_SEQUENCE_NODE) {
    SetError("Node is not a sequence");
    return NULL;
  }

  int count = node->node->data.sequence.items.top - node->node->data.sequence.items.start;
  if (index < 0 || index >= count) {
    SetError("Sequence index %d out of bounds (size: %d)", index, count);
    return NULL;
  }

  yaml_node_item_t item = node->node->data.sequence.items.start[index];
  yaml_node_t *item_node = yaml_document_get_node(node->document, item);
  if (!item_node) {
    SetError("Invalid sequence item at index %d", index);
    return NULL;
  }

  YamlNode *result = AllocNode(node->doc);
  if (!result) {
    return NULL;  // Error already set by AllocNode
  }

  result->doc = node->doc;
  result->document = node->document;
  result->node = item_node;
  return result;
}

int Yaml_GetSequenceLength(YamlNode *node) {
  if (!node || node->node->type != YAML_SEQUENCE_NODE) {
    return 0;
  }

  return node->node->data.sequence.items.top - node->node->data.sequence.items.start;
}

bool Yaml_HasKey(YamlNode *node, const char *key) {
  if (!node || !key || node->node->type != YAML_MAPPING_NODE) {
    return false;
  }

  for (yaml_node_pair_t *pair = node->node->data.mapping.pairs.start;
       pair < node->node->data.mapping.pairs.top; pair++) {

    yaml_node_t *key_node = yaml_document_get_node(node->document, pair->key);
    if (key_node && key_node->type == YAML_SCALAR_NODE) {
      const char *key_str = (const char*)key_node->data.scalar.value;
      if (strcmp(key_str, key) == 0) {
        return true;
      }
    }
  }

  return false;
}

// ============================================================================
// Value Extraction
// ============================================================================

const char* Yaml_GetString(YamlNode *node, const char *key, const char *default_value) {
  YamlNode *value_node = Yaml_GetMapping(node, key);
  if (!value_node) {
    g_yaml_error[0] = '\0';  // Clear error for default returns
    return default_value;
  }

  if (value_node->node->type != YAML_SCALAR_NODE) {
    // Node is managed by pool, no need to free
    return default_value;
  }

  // Node is managed by pool, no need to free
  return (const char*)value_node->node->data.scalar.value;
}

int Yaml_GetInt(YamlNode *node, const char *key, int default_value) {
  const char *str = Yaml_GetString(node, key, NULL);
  if (!str) {
    return default_value;
  }

  char *endptr;
  long value = strtol(str, &endptr, 10);

  if (*endptr != '\0') {
    // Not a valid integer
    return default_value;
  }

  return (int)value;
}

bool Yaml_GetBool(YamlNode *node, const char *key, bool default_value) {
  const char *str = Yaml_GetString(node, key, NULL);
  if (!str) {
    return default_value;
  }

  // Accept various boolean formats
  if (strcmp(str, "true") == 0 || strcmp(str, "yes") == 0 || strcmp(str, "1") == 0) {
    return true;
  }
  if (strcmp(str, "false") == 0 || strcmp(str, "no") == 0 || strcmp(str, "0") == 0) {
    return false;
  }

  return default_value;
}

// ============================================================================
// Direct Value Access
// ============================================================================

const char* Yaml_AsString(YamlNode *node) {
  if (!node || node->node->type != YAML_SCALAR_NODE) {
    return NULL;
  }
  return (const char*)node->node->data.scalar.value;
}

int Yaml_AsInt(YamlNode *node) {
  const char *str = Yaml_AsString(node);
  if (!str) {
    return 0;
  }

  char *endptr;
  long value = strtol(str, &endptr, 10);

  if (*endptr != '\0') {
    return 0;
  }

  return (int)value;
}
