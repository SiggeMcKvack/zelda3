// music_compiler.h - Pure C music compiler (replaces Python compile_music.py)
// Compiles sound bank text files to loadable SPC sequence format
#ifndef MUSIC_COMPILER_H
#define MUSIC_COMPILER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Compile a sound bank for the specified song
// song_name: "intro", "indoor", or "ending"
// assets_path: Path to assets directory (containing sound_*.txt, music_info.yaml, sound/*.brr)
// out_data: Receives pointer to allocated data (caller must free)
// out_size: Receives size of output data
// Returns: true on success, false on error
bool MusicCompiler_CompileSoundBank(const char *song_name, const char *assets_path,
                                    uint8_t **out_data, size_t *out_size);

#endif // MUSIC_COMPILER_H
