# Feature Flags

[Home](../index.md) > [Architecture](../architecture.md) > Feature Flags

## Overview

The feature flag system allows Zelda3 to add enhancements while preserving the original SNES behavior for replay compatibility. Features are stored in unused RAM locations and controlled via `zelda3.ini`, ensuring they can be toggled without recompiling.

## Design Principles

1. **Original behavior by default:** All features disabled unless explicitly enabled
2. **Replay compatible:** Features stored in saved state for consistent playback
3. **User controlled:** All features exposed in INI configuration
4. **RAM-based storage:** Uses unused SNES memory locations (0x648+)
5. **Bitfield flags:** Power-of-2 values allow combining multiple features

## Architecture

### Memory Storage

Feature flags live in unused SNES RAM locations that were never used by the original game:

```c
// src/features.h
enum {
  kRam_Features0 = 0x64c,  // Primary feature bitfield (32-bit)
};

// Access macro (defined in features.h)
#define enhanced_features0 (*(uint32*)(g_ram+0x64c))
```

This storage method provides:
- **Snapshot compatibility:** Features saved/restored with game state
- **Replay accuracy:** Feature state recorded in replay files
- **ROM verification:** Can compare with/without features enabled

### Flag Definitions

Features are defined as power-of-2 constants:

```c
// src/features.h
enum {
  kFeatures0_ExtendScreen64 = 1,           // 2^0
  kFeatures0_SwitchLR = 2,                 // 2^1
  kFeatures0_TurnWhileDashing = 4,         // 2^2
  kFeatures0_MirrorToDarkworld = 8,        // 2^3
  kFeatures0_CollectItemsWithSword = 16,   // 2^4
  kFeatures0_BreakPotsWithSword = 32,      // 2^5
  kFeatures0_DisableLowHealthBeep = 64,    // 2^6
  kFeatures0_SkipIntroOnKeypress = 128,    // 2^7
  kFeatures0_ShowMaxItemsInYellow = 256,   // 2^8
  kFeatures0_MoreActiveBombs = 512,        // 2^9
  kFeatures0_WidescreenVisualFixes = 1024, // 2^10
  kFeatures0_CarryMoreRupees = 2048,       // 2^11
  kFeatures0_MiscBugFixes = 4096,          // 2^12
  kFeatures0_CancelBirdTravel = 8192,      // 2^13
  kFeatures0_GameChangingBugFixes = 16384, // 2^14
  kFeatures0_SwitchLRLimit = 32768,        // 2^15
  kFeatures0_DimFlashes = 65536,           // 2^16
  kFeatures0_Pokemode = 131072,            // 2^17
  kFeatures0_PrincessZeldaHelps = 262144,  // 2^18
};
```

**Capacity:** A 32-bit bitfield supports up to 32 features. When full, add `kRam_Features1` at the next unused offset.

## Adding a New Feature

### Step 1: Define the Flag

Add a new power-of-2 constant to `src/features.h`:

```c
// src/features.h
enum {
  // Existing features...
  kFeatures0_PrincessZeldaHelps = 262144,

  // New feature
  kFeatures0_MyNewFeature = 524288,  // Next power of 2 (2^19)
};
```

### Step 2: Add INI Configuration

Add the feature to `zelda3.ini` template:

```ini
# zelda3.ini
[Features]
# Existing features...
EnablePrincessZeldaHelps = false

# New feature
EnableMyNewFeature = false  # Description of what this does
```

### Step 3: Parse INI Setting

Add parsing code in `src/config.c`:

```c
// src/config.c
static void ParseFeaturesSection(const char *key, const char *value) {
  // Existing feature parsing...
  if (strcmp(key, "EnablePrincessZeldaHelps") == 0) {
    if (ParseBool(value, &enabled))
      SetFeatureBit(kFeatures0_PrincessZeldaHelps, enabled);
  }

  // New feature parsing
  else if (strcmp(key, "EnableMyNewFeature") == 0) {
    bool enabled;
    if (ParseBool(value, &enabled))
      SetFeatureBit(kFeatures0_MyNewFeature, enabled);
  }
}

// Helper function to set/clear bits
static void SetFeatureBit(uint32 flag, bool enabled) {
  if (enabled)
    g_wanted_zelda_features |= flag;
  else
    g_wanted_zelda_features &= ~flag;
}
```

### Step 4: Implement the Feature

Add feature logic to the relevant module, gated by the flag check:

```c
// src/player.c (or relevant module)
void Player_HandleSomeAction() {
  // Original behavior
  if (!(enhanced_features0 & kFeatures0_MyNewFeature)) {
    OriginalBehavior();
    return;
  }

  // Enhanced behavior (only if enabled)
  EnhancedBehavior();
}
```

**Alternative pattern** (adding functionality):

```c
void Player_Update() {
  // Original code runs always
  OriginalUpdate();

  // New feature adds extra functionality
  if (enhanced_features0 & kFeatures0_MyNewFeature) {
    AdditionalUpdate();
  }
}
```

### Step 5: Test

1. Test with feature **disabled** (default):
   ```ini
   EnableMyNewFeature = false
   ```
   Verify original behavior is preserved.

2. Test with feature **enabled**:
   ```ini
   EnableMyNewFeature = true
   ```
   Verify new behavior works correctly.

3. Test snapshot/replay:
   - Save state with feature on
   - Load state with feature off (should fail or warn)
   - Replay should preserve feature state

## Feature Examples

### Example 1: L/R Item Switching

**Feature:** Press L/R to cycle through items without opening menu.

```c
// src/features.h
kFeatures0_SwitchLR = 2,

// src/player.c
void Player_HandleInput() {
  // Check if feature enabled
  if (enhanced_features0 & kFeatures0_SwitchLR) {
    // L button: previous item
    if (joypad_l_new & 0x20) {
      SelectPreviousItem();
    }

    // R button: next item
    if (joypad_r_new & 0x10) {
      SelectNextItem();
    }
  }

  // Original input handling continues...
}
```

### Example 2: Widescreen Support

**Feature:** Extend screen rendering by 64 pixels (32 left, 32 right).

```c
// src/features.h
kFeatures0_ExtendScreen64 = 1,

// snes/ppu.c
void PpuRenderScanline(Ppu *ppu, int y) {
  int width = 256;  // SNES native width

  // Extend width if widescreen enabled
  if (enhanced_features0 & kFeatures0_ExtendScreen64) {
    width += kPpuExtraLeftRight * 2;  // 256 + 64 = 320
  }

  for (int x = 0; x < width; x++) {
    RenderPixel(ppu, x, y);
  }
}
```

### Example 3: Disable Low Health Beep

**Feature:** Silence annoying beep when health is low.

```c
// src/features.h
kFeatures0_DisableLowHealthBeep = 64,

// src/player.c
void Player_UpdateSound() {
  // Original: beep when low health
  if (link_health_current <= 8) {
    // Check if beep is disabled
    if (!(enhanced_features0 & kFeatures0_DisableLowHealthBeep)) {
      PlaySound(kSfx_LowHealth);
    }
  }
}
```

### Example 4: Sword Collects Items

**Feature:** Swinging sword picks up items (rupees, hearts, etc.).

```c
// src/features.h
kFeatures0_CollectItemsWithSword = 16,

// src/ancilla.c (items on ground)
void Ancilla_CheckCollection(int ancilla_id) {
  // Original: only collect when Link touches item
  if (CheckLinkCollision(ancilla_id)) {
    CollectItem(ancilla_id);
    return;
  }

  // New: also collect when sword touches item
  if (enhanced_features0 & kFeatures0_CollectItemsWithSword) {
    if (link_sword_active && CheckSwordCollision(ancilla_id)) {
      CollectItem(ancilla_id);
    }
  }
}
```

### Example 5: Bug Fixes

**Feature:** Fix original SNES bugs that affect gameplay.

```c
// src/features.h
kFeatures0_GameChangingBugFixes = 16384,

// src/sprite_main.c
void Sprite_UpdateSomeEnemy(int sprite_id) {
  // Original code has a bug: wrong sprite index
  if (!(enhanced_features0 & kFeatures0_GameChangingBugFixes)) {
    // Preserve original bug for compatibility
    int buggy_index = sprite_id + 1;  // Off by one!
    UpdateOtherSprite(buggy_index);
  } else {
    // Fixed version
    int correct_index = sprite_id;
    UpdateOtherSprite(correct_index);
  }
}
```

## Configuration Integration

### INI File Structure

```ini
# zelda3.ini
[Features]
# Visual enhancements
ExtendedScreen = true       # Widescreen support
WidescreenVisualFixes = true  # Fix visual glitches in widescreen

# Gameplay improvements
SwitchLR = true             # L/R item switching
TurnWhileDashing = true     # Turn during dash
CollectItemsWithSword = true  # Sword collects items
BreakPotsWithSword = true   # Sword breaks pots

# Quality of life
DisableLowHealthBeep = true  # Silence low health beep
SkipIntroOnKeypress = true   # Skip intro with any button
ShowMaxItemsInYellow = true  # Highlight maxed items

# Bug fixes
MiscBugFixes = true         # Various minor fixes
GameChangingBugFixes = false  # Fixes that alter behavior (off by default!)

# Advanced
MoreActiveBombs = false     # Allow 10+ bombs on screen
CarryMoreRupees = false     # Increase rupee cap to 999,999
```

### Runtime Flag Initialization

```c
// src/main.c
void InitializeGame() {
  // Load config from zelda3.ini
  LoadConfig("zelda3.ini");

  // Copy wanted features to RAM
  enhanced_features0 = g_wanted_zelda_features;

  // Features are now active
}
```

### Snapshot Storage

Feature flags are automatically saved/restored with game state:

```c
// src/zelda_rtl.c
void ZeldaSaveSnapshot(int slot) {
  // g_ram includes enhanced_features0 at offset 0x64c
  SaveToFile(snapshot_file, g_ram, sizeof(g_ram));
}

void ZeldaLoadSnapshot(int slot) {
  LoadFromFile(snapshot_file, g_ram, sizeof(g_ram));
  // enhanced_features0 is restored automatically
}
```

## Best Practices

### 1. Preserve Original Behavior

Always make enhanced behavior opt-in:

```c
// Good: Feature adds new behavior
if (enhanced_features0 & kFeatures0_MyFeature) {
  NewBehavior();
} else {
  OriginalBehavior();
}

// Also good: Feature disables annoying behavior
if (!(enhanced_features0 & kFeatures0_DisableAnnoyance)) {
  AnnoyingBehavior();  // Only runs if NOT disabled
}
```

### 2. Document Feature Impact

Add clear comments explaining what changes:

```c
// Vanilla behavior: Sword doesn't break pots
// Enhanced behavior: Sword breaks pots like in later Zelda games
if (enhanced_features0 & kFeatures0_BreakPotsWithSword) {
  CheckSwordVsPot();
}
```

### 3. Group Related Features

Use clear naming for feature groups:

```c
// Visual features (prefix: none)
kFeatures0_ExtendScreen64
kFeatures0_WidescreenVisualFixes

// Gameplay features (verbs: Switch, Turn, Collect, etc.)
kFeatures0_SwitchLR
kFeatures0_TurnWhileDashing
kFeatures0_CollectItemsWithSword

// Bug fixes (suffix: BugFixes)
kFeatures0_MiscBugFixes
kFeatures0_GameChangingBugFixes
```

### 4. Default to OFF for Compatibility

**Always** default new features to disabled in `zelda3.ini`:

```ini
# Good
EnableMyNewFeature = false  # Off by default

# Bad - breaks replay compatibility
EnableMyNewFeature = true   # Don't do this!
```

### 5. Test Replay Compatibility

Verify replays work correctly:

1. Record replay with feature OFF
2. Play back with feature OFF → should match exactly
3. Play back with feature ON → should fail or warn (desync expected)

## Multiple Feature Bitfields

When `enhanced_features0` is full (32 bits), add a second bitfield:

```c
// src/features.h
enum {
  kRam_Features0 = 0x64c,  // First 32 features
  kRam_Features1 = 0x650,  // Next 32 features (new!)
};

#define enhanced_features0 (*(uint32*)(g_ram+0x64c))
#define enhanced_features1 (*(uint32*)(g_ram+0x650))  // New macro

// New features use features1
enum {
  kFeatures1_MyNewFeature = 1,
  kFeatures1_AnotherFeature = 2,
  // ...
};

// Usage
if (enhanced_features1 & kFeatures1_MyNewFeature) {
  // ...
}
```

## See Also

- **[Architecture](../architecture.md)** - Overall system architecture
- **[Memory Layout](memory-layout.md)** - RAM storage details
- **Code References:**
  - `src/features.h` - Feature flag definitions
  - `src/config.c` - INI parsing and flag initialization
  - `src/variables.h` - RAM macro definitions
  - `zelda3.ini` - User configuration

**User Documentation:**
- [Getting Started](../getting-started.md) - Enabling features
- [Troubleshooting](../troubleshooting.md) - Feature-related issues
