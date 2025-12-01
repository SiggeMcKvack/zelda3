#ifndef ZELDA3_PLATFORM_H_
#define ZELDA3_PLATFORM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Platform abstraction layer for file I/O and other platform-specific operations

// ============================================================================
// General File I/O (relative to current working directory)
// ============================================================================

// File handle abstraction
typedef struct PlatformFile PlatformFile;

// File I/O operations
PlatformFile *Platform_OpenFile(const char *filename, const char *mode);
size_t Platform_ReadFile(void *ptr, size_t size, size_t count, PlatformFile *file);
size_t Platform_WriteFile(const void *ptr, size_t size, size_t count, PlatformFile *file);
int Platform_SeekFile(PlatformFile *file, long offset, int whence);
long Platform_TellFile(PlatformFile *file);
int Platform_CloseFile(PlatformFile *file);
int Platform_EofFile(PlatformFile *file);

// Utility function to read entire file into memory
uint8_t *Platform_ReadWholeFile(const char *filename, size_t *length_out);

// Case-insensitive path lookup
// On case-insensitive filesystems (Windows, macOS), returns the input path unchanged
// On case-sensitive filesystems (Unix/Linux), searches for a case-insensitive match
// Returns a newly allocated string with the corrected path, or NULL if not found
// Caller must free() the returned string
char *Platform_FindFileWithCaseInsensitivity(const char *path);

// Platform initialization (for platforms that need it)
void Platform_Init(void);
void Platform_Shutdown(void);

// ============================================================================
// Save File I/O (platform-specific save directory)
// On desktop: saves/ subdirectory
// On Android: SAF external storage via JNI
// ============================================================================

// Opaque save file handle
typedef struct PlatformSaveFile PlatformSaveFile;

// Open a save file for reading or writing
// filename: Just the filename (e.g., "save1.sav", "sram.dat")
// for_writing: true = create/truncate for write, false = open for read
// Returns NULL on failure
PlatformSaveFile *Platform_OpenSaveFile(const char *filename, bool for_writing);

// Read from save file (like fread)
size_t Platform_ReadSaveFile(void *ptr, size_t size, size_t count, PlatformSaveFile *file);

// Write to save file (like fwrite)
size_t Platform_WriteSaveFile(const void *ptr, size_t size, size_t count, PlatformSaveFile *file);

// Close save file
int Platform_CloseSaveFile(PlatformSaveFile *file);

// Get underlying FILE* pointer (for compatibility with legacy APIs)
// The caller should NOT fclose() this - use Platform_CloseSaveFile instead
FILE *Platform_GetSaveFileHandle(PlatformSaveFile *file);

// Check if save file exists
bool Platform_SaveFileExists(const char *filename);

// Delete a save file
bool Platform_DeleteSaveFile(const char *filename);

// Rename a save file (for backup rotation)
bool Platform_RenameSaveFile(const char *old_name, const char *new_name);

// ============================================================================
// Path Utilities
// ============================================================================

// Get the saves directory path (creates if needed)
// Returns static buffer - do not free
// Desktop: "saves/", Android: "" (SAF handles paths internally)
const char *Platform_GetSaveDirectory(void);

#endif  // ZELDA3_PLATFORM_H_
