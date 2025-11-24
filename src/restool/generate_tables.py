#!/usr/bin/env python3
"""
Generate tables.c from assets/tables.py
This is a build-time helper script, not part of the runtime.
"""
import sys
sys.path.insert(0, '../../assets')
import tables

def escape_c_string(s):
    """Escape a string for C"""
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')

def write_string_array(f, name, count, items):
    """Write a C string array"""
    f.write(f'const char *{name}[{count}] = {{\n')
    for i, item in enumerate(items):
        escaped = escape_c_string(item)
        comma = ',' if i < len(items) - 1 else ''
        f.write(f'  "{escaped}"{comma}\n')
    f.write('};\n\n')

def write_music_entries(f):
    """Write music entries (sparse mapping)"""
    f.write('const MusicEntry kMusicEntries[kMusicEntriesCount] = {\n')
    items = sorted(tables.kMusicNames.items())
    for i, (index, name) in enumerate(items):
        comma = ',' if i < len(items) - 1 else ''
        f.write(f'  {{{index}, "{escape_c_string(name)}"}}{comma}\n')
    f.write('};\n\n')

def write_ambient_sound_entries(f):
    """Write ambient sound entries (sparse mapping)"""
    f.write('const AmbientSoundEntry kAmbientSoundEntries[kAmbientSoundEntriesCount] = {\n')
    items = sorted(tables.kAmbientSoundName.items())
    for i, (index, name) in enumerate(items):
        comma = ',' if i < len(items) - 1 else ''
        f.write(f'  {{{index}, "{escape_c_string(name)}"}}{comma}\n')
    f.write('};\n\n')

def write_lookup_function(f, array_name, count_name, func_name):
    """Write a linear lookup function"""
    f.write(f'''int {func_name}(const char *name) {{
  if (!name) return -1;
  for (int i = 0; i < {count_name}; i++) {{
    if (strcmp({array_name}[i], name) == 0) {{
      return i;
    }}
  }}
  return -1;
}}

''')

def write_sparse_lookup_functions(f):
    """Write sparse lookup functions for music/ambient"""
    f.write('''int FindMusicIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kMusicEntriesCount; i++) {
    if (strcmp(kMusicEntries[i].name, name) == 0) {
      return kMusicEntries[i].index;
    }
  }
  return -1;
}

int FindAmbientSoundIndex(const char *name) {
  if (!name) return -1;
  for (int i = 0; i < kAmbientSoundEntriesCount; i++) {
    if (strcmp(kAmbientSoundEntries[i].name, name) == 0) {
      return kAmbientSoundEntries[i].index;
    }
  }
  return -1;
}

const char* GetMusicName(int index) {
  for (int i = 0; i < kMusicEntriesCount; i++) {
    if (kMusicEntries[i].index == index) {
      return kMusicEntries[i].name;
    }
  }
  return NULL;
}

const char* GetAmbientSoundName(int index) {
  for (int i = 0; i < kAmbientSoundEntriesCount; i++) {
    if (kAmbientSoundEntries[i].index == index) {
      return kAmbientSoundEntries[i].name;
    }
  }
  return NULL;
}

''')

def main():
    with open('tables.c', 'w') as f:
        f.write('''// tables.c - Lookup tables for YAML asset extraction
// AUTO-GENERATED from assets/tables.py by generate_tables.py
// DO NOT EDIT MANUALLY
#include "tables.h"
#include <string.h>

// ============================================================================
// Object Type Names
// ============================================================================

''')

        # Type 0 names
        write_string_array(f, 'kType0Names', 'kType0NamesCount', tables.kType0Names)

        # Type 1 names
        write_string_array(f, 'kType1Names', 'kType1NamesCount', tables.kType1Names)

        # Type 2 names
        write_string_array(f, 'kType2Names', 'kType2NamesCount', tables.kType2Names)

        f.write('// ============================================================================\n')
        f.write('// Sprite Names\n')
        f.write('// ============================================================================\n\n')

        write_string_array(f, 'kSpriteNames', 'kSpriteNamesCount', tables.kSpriteNames)

        f.write('// ============================================================================\n')
        f.write('// Door Tag Names\n')
        f.write('// ============================================================================\n\n')

        write_string_array(f, 'kTagNames', 'kTagNamesCount', tables.kTagNames)

        f.write('// ============================================================================\n')
        f.write('// Room Property Names\n')
        f.write('// ============================================================================\n\n')

        write_string_array(f, 'kEffectNames', 'kEffectNamesCount', tables.kEffectNames)
        write_string_array(f, 'kCollisionNames', 'kCollisionNamesCount', tables.kCollisionNames)
        write_string_array(f, 'kBg2', 'kBg2Count', tables.kBg2)

        f.write('// ============================================================================\n')
        f.write('// Audio Names (sparse mappings)\n')
        f.write('// ============================================================================\n\n')

        write_music_entries(f)
        write_ambient_sound_entries(f)

        f.write('// ============================================================================\n')
        f.write('// Palace/Dungeon Names\n')
        f.write('// ============================================================================\n\n')

        write_string_array(f, 'kPalaceNames', 'kPalaceNamesCount', tables.kPalaceNames)

        f.write('// ============================================================================\n')
        f.write('// Lookup Functions\n')
        f.write('// ============================================================================\n\n')

        # Write lookup functions
        write_lookup_function(f, 'kType0Names', 'kType0NamesCount', 'FindType0Index')
        write_lookup_function(f, 'kType1Names', 'kType1NamesCount', 'FindType1Index')
        write_lookup_function(f, 'kType2Names', 'kType2NamesCount', 'FindType2Index')
        write_lookup_function(f, 'kSpriteNames', 'kSpriteNamesCount', 'FindSpriteIndex')
        write_lookup_function(f, 'kTagNames', 'kTagNamesCount', 'FindTagIndex')
        write_lookup_function(f, 'kEffectNames', 'kEffectNamesCount', 'FindEffectIndex')
        write_lookup_function(f, 'kCollisionNames', 'kCollisionNamesCount', 'FindCollisionIndex')
        write_lookup_function(f, 'kBg2', 'kBg2Count', 'FindBg2Index')
        write_lookup_function(f, 'kPalaceNames', 'kPalaceNamesCount', 'FindPalaceIndex')

        write_sparse_lookup_functions(f)

    print('Generated tables.c successfully')
    print(f'  kType0Names: {len(tables.kType0Names)} entries')
    print(f'  kType1Names: {len(tables.kType1Names)} entries')
    print(f'  kType2Names: {len(tables.kType2Names)} entries')
    print(f'  kSpriteNames: {len(tables.kSpriteNames)} entries')
    print(f'  kTagNames: {len(tables.kTagNames)} entries')
    print(f'  kEffectNames: {len(tables.kEffectNames)} entries')
    print(f'  kCollisionNames: {len(tables.kCollisionNames)} entries')
    print(f'  kBg2: {len(tables.kBg2)} entries')
    print(f'  kMusicEntries: {len(tables.kMusicNames)} entries')
    print(f'  kAmbientSoundEntries: {len(tables.kAmbientSoundName)} entries')
    print(f'  kPalaceNames: {len(tables.kPalaceNames)} entries')
    print(f'Total: ~{sum([len(tables.kType0Names), len(tables.kType1Names), len(tables.kType2Names), len(tables.kSpriteNames), len(tables.kTagNames), len(tables.kEffectNames), len(tables.kCollisionNames), len(tables.kBg2), len(tables.kMusicNames), len(tables.kAmbientSoundName), len(tables.kPalaceNames)])} entries')

if __name__ == '__main__':
    main()
