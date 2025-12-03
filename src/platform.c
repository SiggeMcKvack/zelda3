#include "platform.h"
#include "types.h"
#include "platform_detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#if defined(PLATFORM_POSIX)
#include <dirent.h>
#include <strings.h>  // For strcasecmp
#endif

#ifdef PLATFORM_ANDROID
#include <android/log.h>
#include <unistd.h>
#include "platform/android/android_jni.h"

// Check if path is for external storage (needs SAF)
static bool IsExternalPath(const char *path) {
  return strncmp(path, "MSU/", 4) == 0 ||
         strncmp(path, "shaders/", 8) == 0;
  // Note: saves use Platform_OpenSaveFile(), not Platform_OpenFile()
}
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
#ifdef PLATFORM_ANDROID
  // External paths route through SAF (Storage Access Framework)
  if (IsExternalPath(filename)) {
    int fd = Android_OpenExternalFile(filename, mode);
    if (fd < 0)
      return NULL;

    FILE *fp = fdopen(fd, mode);
    if (!fp) {
      close(fd);
      return NULL;
    }

    PlatformFile *pf = (PlatformFile *)malloc(sizeof(PlatformFile));
    if (!pf) {
      fclose(fp);
      return NULL;
    }
    pf->fp = fp;
    return pf;
  }
#endif

  // Regular file (app-internal or desktop)
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
#ifdef PLATFORM_ANDROID
  __android_log_print(ANDROID_LOG_DEBUG, "Zelda3Platform",
                      "Platform_ReadWholeFile: Attempting to read '%s'", filename);
#endif

  PlatformFile *file = Platform_OpenFile(filename, "rb");
  if (!file) {
#ifdef PLATFORM_ANDROID
    __android_log_print(ANDROID_LOG_ERROR, "Zelda3Platform",
                        "Platform_ReadWholeFile: Failed to open '%s' (errno=%d: %s)",
                        filename, errno, strerror(errno));
#endif
    return NULL;
  }

#ifdef PLATFORM_ANDROID
  __android_log_print(ANDROID_LOG_DEBUG, "Zelda3Platform",
                      "Platform_ReadWholeFile: Successfully opened '%s'", filename);
#endif

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

// ============================================================================
// Save File API Implementation (Desktop)
// ============================================================================

#ifndef PLATFORM_ANDROID

static const char *kSaveDirectory = "saves/";

struct PlatformSaveFile {
  FILE *fp;
};

const char *Platform_GetSaveDirectory(void) {
  return kSaveDirectory;
}

PlatformSaveFile *Platform_OpenSaveFile(const char *filename, bool for_writing) {
  char path[512];
  snprintf(path, sizeof(path), "%s%s", kSaveDirectory, filename);

  FILE *fp = fopen(path, for_writing ? "wb" : "rb");
  if (!fp)
    return NULL;

  PlatformSaveFile *sf = (PlatformSaveFile *)malloc(sizeof(PlatformSaveFile));
  if (!sf) {
    fclose(fp);
    return NULL;
  }
  sf->fp = fp;
  return sf;
}

size_t Platform_ReadSaveFile(void *ptr, size_t size, size_t count, PlatformSaveFile *file) {
  if (!file || !file->fp)
    return 0;
  return fread(ptr, size, count, file->fp);
}

size_t Platform_WriteSaveFile(const void *ptr, size_t size, size_t count, PlatformSaveFile *file) {
  if (!file || !file->fp)
    return 0;
  return fwrite(ptr, size, count, file->fp);
}

int Platform_CloseSaveFile(PlatformSaveFile *file) {
  if (!file)
    return -1;
  int result = 0;
  if (file->fp)
    result = fclose(file->fp);
  free(file);
  return result;
}

FILE *Platform_GetSaveFileHandle(PlatformSaveFile *file) {
  if (!file)
    return NULL;
  return file->fp;
}

bool Platform_SaveFileExists(const char *filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s%s", kSaveDirectory, filename);
  struct stat st;
  return stat(path, &st) == 0;
}

bool Platform_DeleteSaveFile(const char *filename) {
  char path[512];
  snprintf(path, sizeof(path), "%s%s", kSaveDirectory, filename);
  return remove(path) == 0;
}

bool Platform_RenameSaveFile(const char *old_name, const char *new_name) {
  char old_path[512], new_path[512];
  snprintf(old_path, sizeof(old_path), "%s%s", kSaveDirectory, old_name);
  snprintf(new_path, sizeof(new_path), "%s%s", kSaveDirectory, new_name);
  return rename(old_path, new_path) == 0;
}

#endif  // !PLATFORM_ANDROID

// ============================================================================
// Case-insensitive path lookup
// ============================================================================

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
