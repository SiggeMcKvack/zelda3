#ifndef ZELDA3_DYNAMIC_ARRAY_H_
#define ZELDA3_DYNAMIC_ARRAY_H_

#include <stdlib.h>
#include <stdbool.h>

// Simple type-safe growable array macros
//
// These macros provide a lightweight alternative to duplicating realloc logic.
// Each call site can choose its own growth strategy and error handling.
//
// Example usage (config.c style - grow by fixed steps):
//   MyType *array = NULL;
//   size_t count = 0;
//
//   // Check if growth needed (every 256 items)
//   if ((count & 0xff) == 0) {
//     DYNARR_GROW_STEP(array, count, 256, {
//       LogError("Out of memory");
//       return false;
//     });
//   }
//   array[count++] = value;
//
// Example usage (util.c ByteArray style - exponential growth):
//   if (size > capacity) {
//     size_t min_capacity = capacity + (capacity >> 1) + 8;
//     size_t new_capacity = size < min_capacity ? min_capacity : size;
//     DYNARR_REALLOC(data, new_capacity, { Die("OOM"); });
//     capacity = new_capacity;
//   }

// Grow array by fixed step size
// Rounds up current size to next multiple of step_size
#define DYNARR_GROW_STEP(arr, current_size, step_size, on_fail) do { \
  size_t _new_cap = ((current_size) + (step_size)); \
  void *_new_arr = realloc((arr), sizeof(*(arr)) * _new_cap); \
  if (!_new_arr) { \
    on_fail; \
  } \
  (arr) = _new_arr; \
} while (0)

// Reallocate array to specific capacity
#define DYNARR_REALLOC(arr, new_capacity, on_fail) do { \
  void *_new_arr = realloc((arr), sizeof(*(arr)) * (new_capacity)); \
  if (!_new_arr) { \
    on_fail; \
  } \
  (arr) = _new_arr; \
} while (0)

// Free and nullify array pointer
#define DYNARR_FREE(arr) do { \
  free(arr); \
  (arr) = NULL; \
} while (0)

#endif  // ZELDA3_DYNAMIC_ARRAY_H_
