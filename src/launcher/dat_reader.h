// dat_reader.h - Lightweight DAT file reader for launcher
// Reads language info without loading full game assets
#ifndef DAT_READER_H
#define DAT_READER_H

#include <stdbool.h>

// Check if zelda3_assets.dat exists in the given directory
bool DatReader_Exists(const char *dir);

// Get available languages from DAT file
// dir: directory containing zelda3_assets.dat
// languages: output array for language codes (e.g., "us", "de", "retrans-kal")
// max_languages: size of languages array
// Returns: number of languages found, 0 on error
int DatReader_GetLanguages(const char *dir, char languages[][16], int max_languages);

#endif // DAT_READER_H
