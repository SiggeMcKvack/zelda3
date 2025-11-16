#ifndef ZELDA3_PLATFORM_DETECT_H_
#define ZELDA3_PLATFORM_DETECT_H_

// Platform detection header
// Provides consistent platform macros across the codebase
// Use these instead of raw compiler defines

// Primary platform detection
#if defined(_WIN32) || defined(_WIN64)
  #define PLATFORM_WINDOWS 1
#elif defined(__ANDROID__)
  #define PLATFORM_ANDROID 1
  #define PLATFORM_POSIX 1
#elif defined(__SWITCH__)
  #define PLATFORM_SWITCH 1
#elif defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IPHONE
    #define PLATFORM_IOS 1
  #else
    #define PLATFORM_MACOS 1
  #endif
  #define PLATFORM_POSIX 1
#elif defined(__linux__)
  #define PLATFORM_LINUX 1
  #define PLATFORM_POSIX 1
#elif defined(__unix__) || defined(__unix)
  #define PLATFORM_UNIX 1
  #define PLATFORM_POSIX 1
#else
  #define PLATFORM_UNKNOWN 1
#endif

// Compiler detection
#if defined(_MSC_VER)
  #define COMPILER_MSVC 1
#elif defined(__clang__)
  #define COMPILER_CLANG 1
#elif defined(__GNUC__)
  #define COMPILER_GCC 1
#endif

// Architecture detection
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
  #define ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
  #define ARCH_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__)
  #define ARCH_ARM64 1
#elif defined(_M_ARM) || defined(__arm__)
  #define ARCH_ARM 1
#endif

#endif  // ZELDA3_PLATFORM_DETECT_H_
