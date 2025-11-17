#ifndef ZELDA3_PLATFORM_H_
#define ZELDA3_PLATFORM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Platform abstraction layer for file I/O and other platform-specific operations

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

#endif  // ZELDA3_PLATFORM_H_
