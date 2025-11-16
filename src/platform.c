#include "platform.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Default implementation using standard C FILE*
// Platform-specific implementations can override these functions

struct PlatformFile {
  FILE *fp;
};

void Platform_Init(void) {
  // Default: no initialization needed
}

void Platform_Shutdown(void) {
  // Default: no cleanup needed
}

PlatformFile *Platform_OpenFile(const char *filename, const char *mode) {
  FILE *fp = fopen(filename, mode);
  if (!fp)
    return NULL;

  PlatformFile *pf = (PlatformFile *)malloc(sizeof(PlatformFile));
  if (!pf) {
    fclose(fp);
    return NULL;
  }
  pf->fp = fp;
  return pf;
}

size_t Platform_ReadFile(void *ptr, size_t size, size_t count, PlatformFile *file) {
  if (!file || !file->fp)
    return 0;
  return fread(ptr, size, count, file->fp);
}

size_t Platform_WriteFile(const void *ptr, size_t size, size_t count, PlatformFile *file) {
  if (!file || !file->fp)
    return 0;
  return fwrite(ptr, size, count, file->fp);
}

int Platform_SeekFile(PlatformFile *file, long offset, int whence) {
  if (!file || !file->fp)
    return -1;
  return fseek(file->fp, offset, whence);
}

long Platform_TellFile(PlatformFile *file) {
  if (!file || !file->fp)
    return -1;
  return ftell(file->fp);
}

int Platform_CloseFile(PlatformFile *file) {
  if (!file)
    return -1;
  int result = 0;
  if (file->fp)
    result = fclose(file->fp);
  free(file);
  return result;
}

int Platform_EofFile(PlatformFile *file) {
  if (!file || !file->fp)
    return 1;
  return feof(file->fp);
}

uint8_t *Platform_ReadWholeFile(const char *filename, size_t *length_out) {
  PlatformFile *file = Platform_OpenFile(filename, "rb");
  if (!file)
    return NULL;

  // Get file size
  Platform_SeekFile(file, 0, SEEK_END);
  long size = Platform_TellFile(file);
  Platform_SeekFile(file, 0, SEEK_SET);

  if (size < 0) {
    Platform_CloseFile(file);
    return NULL;
  }

  // Check for overflow when converting long to size_t and adding 1
  // On 32-bit systems, files > 2GB could overflow size_t
  // On 64-bit systems, this is less likely but still possible with corrupted ftell
  if ((unsigned long)size >= SIZE_MAX) {
    Platform_CloseFile(file);
    return NULL;
  }

  // Allocate buffer (size + 1 for null terminator)
  uint8_t *data = (uint8_t *)malloc((size_t)size + 1);
  if (!data) {
    Platform_CloseFile(file);
    return NULL;
  }

  // Read file
  size_t bytes_read = Platform_ReadFile(data, 1, size, file);
  Platform_CloseFile(file);

  if (bytes_read != (size_t)size) {
    free(data);
    return NULL;
  }

  data[size] = 0;  // Null terminate for convenience

  if (length_out)
    *length_out = (size_t)size;

  return data;
}
