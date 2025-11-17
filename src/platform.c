#include "platform.h"
#include "types.h"
#include "platform_detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#if defined(PLATFORM_UNIX)
#include <dirent.h>
#include <strings.h>  // For strcasecmp
#endif

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

char *Platform_FindFileWithCaseInsensitivity(const char *path) {
  if (!path)
    return NULL;

#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_MACOS)
  // Windows and macOS filesystems are case-insensitive by default
  // Just check if the file exists and return a copy of the path
  struct stat st;
  if (stat(path, &st) == 0) {
    return strdup(path);
  }
  return NULL;
#else
  // Unix/Linux: filesystems are typically case-sensitive
  // First check if the path exists exactly as given
  struct stat st;
  if (stat(path, &st) == 0) {
    return strdup(path);
  }

  // Path doesn't exist - try to find it with different case
  // Extract directory and filename
  char *path_copy = strdup(path);
  if (!path_copy)
    return NULL;

  char *last_slash = strrchr(path_copy, '/');
  if (!last_slash) {
    // No directory component, just a filename in current directory
    free(path_copy);

    DIR *dir = opendir(".");
    if (!dir)
      return NULL;

    struct dirent *entry;
    char *result = NULL;
    while ((entry = readdir(dir)) != NULL) {
      if (strcasecmp(entry->d_name, path) == 0) {
        result = strdup(entry->d_name);
        break;
      }
    }
    closedir(dir);
    return result;
  }

  // Split into directory and filename
  *last_slash = '\0';
  const char *dir_path = path_copy;
  const char *filename = last_slash + 1;

  // Open directory and search for case-insensitive match
  DIR *dir = opendir(dir_path);
  if (!dir) {
    free(path_copy);
    return NULL;
  }

  struct dirent *entry;
  char *result = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcasecmp(entry->d_name, filename) == 0) {
      // Found a match - build full path
      size_t len = strlen(dir_path) + 1 + strlen(entry->d_name) + 1;
      result = (char *)malloc(len);
      if (result) {
        snprintf(result, len, "%s/%s", dir_path, entry->d_name);
      }
      break;
    }
  }

  closedir(dir);
  free(path_copy);
  return result;
#endif
}
