#ifndef ZELDA3_DEBUG_STATE_H_
#define ZELDA3_DEBUG_STATE_H_

#include "types.h"

// Debug state tracking for event-driven console output in debug builds
// Only active when kDebugFlag is set (debug builds)

// Tracks previous frame state to detect changes
typedef struct DebugState {
  uint8 prev_main_module;
  uint8 prev_submodule;
  uint16 prev_room_or_area;
  uint8 prev_is_indoors;
  uint8 prev_sprite_count;
  bool initialized;
} DebugState;

// Initialize debug state tracker
void DebugState_Init(DebugState *state);

// Update and log changes (call once per frame after game logic)
void DebugState_Update(DebugState *state);

// Log current state snapshot
void DebugState_LogSnapshot(const char *reason);

#endif  // ZELDA3_DEBUG_STATE_H_
