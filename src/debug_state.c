#include "debug_state.h"

#ifndef NDEBUG

#include "variables.h"
#include "logging.h"
#include <stdio.h>

// Module name lookup for readable output
static const char* GetModuleName(uint8 module_index) {
  static const char* module_names[] = {
    "Intro", "FileSelect", "Module02", "Module03", "Module04", "Module05",
    "PreDungeon", "Dungeon", "Module08", "Overworld", "Module0A", "Module0B",
    "GameOver", "Module0D", "Interface", "Module0F", "Module10", "Module11",
    "Module12", "TriforceRoom", "Module14", "Module15", "Module16", "Module17",
    "Module18", "Module19", "Credits", "Module1B"
  };

  if (module_index < sizeof(module_names) / sizeof(module_names[0])) {
    return module_names[module_index];
  }
  return "Unknown";
}

void DebugState_Init(DebugState *state) {
  state->prev_main_module = main_module_index;
  state->prev_submodule = submodule_index;
  state->prev_room_or_area = player_is_indoors ? dungeon_room_index : overworld_screen_index;
  state->prev_is_indoors = player_is_indoors;
  state->prev_sprite_count = 0;

  // Count active sprites
  for (int k = 0; k < 16; k++) {
    if (sprite_state[k] >= 9) {
      state->prev_sprite_count++;
    }
  }

  state->initialized = true;

  LogDebug("[Frame: %d] Debug state tracking initialized", frame_counter);
  DebugState_LogSnapshot("Initial state");
}

void DebugState_LogSnapshot(const char *reason) {
  // Count active sprites
  uint8 sprite_count = 0;
  for (int k = 0; k < 16; k++) {
    if (sprite_state[k] >= 9) {
      sprite_count++;
    }
  }

  // Count active ancillae
  uint8 ancilla_count = 0;
  for (int k = 0; k < 10; k++) {
    if (ancilla_type[k] != 0) {
      ancilla_count++;
    }
  }

  LogDebug("[Frame: %d] %s", frame_counter, reason);
  LogDebug("  Module: %s (%d/%d)", GetModuleName(main_module_index),
           main_module_index, submodule_index);
  LogDebug("  Location: %s 0x%04X", player_is_indoors ? "Room" : "Area",
           player_is_indoors ? dungeon_room_index : overworld_screen_index);
  LogDebug("  Link: Pos=(%d,%d,%d) Health=%d/%d Dir=%d State=%d",
           link_x_coord, link_y_coord, link_z_coord,
           link_health_current, link_health_capacity,
           link_direction_facing, link_player_handler_state);
  LogDebug("  Objects: %d sprites, %d ancillae", sprite_count, ancilla_count);

  // List active sprite types if any
  if (sprite_count > 0) {
    char sprite_list[256] = "  Sprite types: [";
    int offset = 17; // strlen("  Sprite types: [")
    for (int k = 0; k < 16 && offset < 240; k++) {
      if (sprite_state[k] >= 9) {
        offset += snprintf(sprite_list + offset, sizeof(sprite_list) - offset,
                          "0x%02X ", sprite_type[k]);
      }
    }
    snprintf(sprite_list + offset, sizeof(sprite_list) - offset, "]");
    LogDebug("%s", sprite_list);
  }
}

void DebugState_Update(DebugState *state) {
  if (!state->initialized) {
    DebugState_Init(state);
    return;
  }

  bool logged_something = false;

  // Check for module change
  if (state->prev_main_module != main_module_index ||
      state->prev_submodule != submodule_index) {
    LogDebug("[Frame: %d] Module change: %s (%d/%d) -> %s (%d/%d)",
             frame_counter,
             GetModuleName(state->prev_main_module), state->prev_main_module, state->prev_submodule,
             GetModuleName(main_module_index), main_module_index, submodule_index);
    state->prev_main_module = main_module_index;
    state->prev_submodule = submodule_index;
    logged_something = true;
  }

  // Check for room/area change
  uint16 current_room_or_area = player_is_indoors ? dungeon_room_index : overworld_screen_index;
  if (state->prev_room_or_area != current_room_or_area ||
      state->prev_is_indoors != player_is_indoors) {

    const char* prev_type = state->prev_is_indoors ? "Room" : "Area";
    const char* curr_type = player_is_indoors ? "Room" : "Area";

    LogDebug("[Frame: %d] Location change: %s 0x%04X -> %s 0x%04X%s",
             frame_counter,
             prev_type, state->prev_room_or_area,
             curr_type, current_room_or_area,
             player_is_indoors ? (dung_cur_floor > 0 ? " (dungeon)" : "") : " (overworld)");

    state->prev_room_or_area = current_room_or_area;
    state->prev_is_indoors = player_is_indoors;
    logged_something = true;
  }

  // Check for sprite count change
  uint8 current_sprite_count = 0;
  for (int k = 0; k < 16; k++) {
    if (sprite_state[k] >= 9) {
      current_sprite_count++;
    }
  }

  if (state->prev_sprite_count != current_sprite_count) {
    if (current_sprite_count > state->prev_sprite_count) {
      LogDebug("[Frame: %d] Sprite spawned: %d -> %d active",
               frame_counter, state->prev_sprite_count, current_sprite_count);
    } else {
      LogDebug("[Frame: %d] Sprite removed: %d -> %d active",
               frame_counter, state->prev_sprite_count, current_sprite_count);
    }
    state->prev_sprite_count = current_sprite_count;
    logged_something = true;
  }

  // Log snapshot after any change
  if (logged_something) {
    DebugState_LogSnapshot("Current state");
  }
}

#else  // NDEBUG

// Stub implementations for release builds
void DebugState_Init(DebugState *state) { (void)state; }
void DebugState_Update(DebugState *state) { (void)state; }
void DebugState_LogSnapshot(const char *reason) { (void)reason; }

#endif  // NDEBUG
