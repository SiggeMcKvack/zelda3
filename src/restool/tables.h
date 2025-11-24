// tables.h - Lookup tables for YAML asset extraction
// Ported from assets/tables.py
#ifndef RESTOOL_TABLES_H
#define RESTOOL_TABLES_H

#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Object Type Names (for dungeon objects)
// ============================================================================

// Standard objects (0x00-0xF7)
#define kType0NamesCount 248
extern const char *kType0Names[kType0NamesCount];

// Extended objects (0xF80-0xFFF)
#define kType1NamesCount 128
extern const char *kType1Names[kType1NamesCount];

// Type 2 objects (0x100-0x140)
#define kType2NamesCount 65
extern const char *kType2Names[kType2NamesCount];

// Lookup functions
int FindType0Index(const char *name);  // Returns -1 if not found
int FindType1Index(const char *name);  // Returns -1 if not found
int FindType2Index(const char *name);  // Returns -1 if not found

// ============================================================================
// Sprite Names (for dungeon sprites)
// ============================================================================

#define kSpriteNamesCount 284
extern const char *kSpriteNames[kSpriteNamesCount];

// Lookup function
int FindSpriteIndex(const char *name);  // Returns -1 if not found

// ============================================================================
// Door Tag Names
// ============================================================================

#define kTagNamesCount 64
extern const char *kTagNames[kTagNamesCount];

// Lookup function
int FindTagIndex(const char *name);  // Returns -1 if not found

// ============================================================================
// Room Property Names
// ============================================================================

#define kEffectNamesCount 8
extern const char *kEffectNames[kEffectNamesCount];

#define kCollisionNamesCount 5
extern const char *kCollisionNames[kCollisionNamesCount];

#define kBg2Count 9
extern const char *kBg2[kBg2Count];

// Lookup functions
int FindEffectIndex(const char *name);
int FindCollisionIndex(const char *name);
int FindBg2Index(const char *name);

// ============================================================================
// Audio Names (sparse index mappings)
// ============================================================================

// Music tracks (sparse mapping, use FindMusicIndex)
typedef struct {
  int index;
  const char *name;
} MusicEntry;

#define kMusicEntriesCount 40
extern const MusicEntry kMusicEntries[kMusicEntriesCount];

// Ambient sounds (sparse mapping, use FindAmbientSoundIndex)
typedef struct {
  int index;
  const char *name;
} AmbientSoundEntry;

#define kAmbientSoundEntriesCount 9
extern const AmbientSoundEntry kAmbientSoundEntries[kAmbientSoundEntriesCount];

// Lookup functions (handle sparse indices)
int FindMusicIndex(const char *name);        // Returns -1 if not found
int FindAmbientSoundIndex(const char *name); // Returns -1 if not found
const char* GetMusicName(int index);         // Returns NULL if not found
const char* GetAmbientSoundName(int index);  // Returns NULL if not found

// ============================================================================
// Palace/Dungeon Names
// ============================================================================

#define kPalaceNamesCount 15
extern const char *kPalaceNames[kPalaceNamesCount];

int FindPalaceIndex(const char *name);

// ============================================================================
// Secret Items (Dungeon secrets - sprite drops + special triggers)
// ============================================================================

// Total: 28 entries (22 drops + 6 special)
typedef struct {
  int index;
  const char *name;
} SecretEntry;

extern const SecretEntry kSecretEntries[];
extern const int kSecretEntriesCount;

// Find secret index by name (returns -1 if not found)
int FindSecretIndex(const char *name);

#endif // RESTOOL_TABLES_H
