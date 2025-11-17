#ifndef ZELDA3_MATH_UTIL_H_
#define ZELDA3_MATH_UTIL_H_

#include <stdint.h>

// Math utility functions
// Consolidates math helpers scattered across the codebase

// MSVC doesn't support C99 inline keyword in C mode
#ifndef MATH_INLINE
  #if defined(_MSC_VER)
    #define MATH_INLINE static __inline
  #else
    #define MATH_INLINE static inline
  #endif
#endif

// Absolute value functions
MATH_INLINE uint16_t abs16(uint16_t t) {
  return (t & 0x8000) ? -t : t;
}

MATH_INLINE uint8_t abs8(uint8_t t) {
  return (t & 0x80) ? -t : t;
}

// Min/max functions for signed integers
MATH_INLINE int IntMin(int a, int b) {
  return a < b ? a : b;
}

MATH_INLINE int IntMax(int a, int b) {
  return a > b ? a : b;
}

// Min/max functions for unsigned integers
MATH_INLINE unsigned int UintMin(unsigned int a, unsigned int b) {
  return a < b ? a : b;
}

MATH_INLINE unsigned int UintMax(unsigned int a, unsigned int b) {
  return a > b ? a : b;
}

// Clamp value to range [min, max]
MATH_INLINE int IntClamp(int val, int min, int max) {
  return val < min ? min : (val > max ? max : val);
}

MATH_INLINE unsigned int UintClamp(unsigned int val, unsigned int min, unsigned int max) {
  return val < min ? min : (val > max ? max : val);
}

// Count set bits (population count)
MATH_INLINE int CountBits32(uint32_t n) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcount(n);
#else
  // Brian Kernighan's algorithm
  int count = 0;
  for (; n != 0; count++)
    n &= (n - 1);
  return count;
#endif
}

// Approximates atan2(y, x) normalized to the [0,4) range
// Maximum error of 0.1620 degrees
// Uses: normalized_atan(x) ~ (b x + x^2) / (1 + 2 b x + x^2)
MATH_INLINE float ApproximateAtan2(float y, float x) {
  uint32_t sign_mask = 0x80000000;
  float b = 0.596227f;
  // Extract the sign bits
  uint32_t ux_s = sign_mask & *(uint32_t *)&x;
  uint32_t uy_s = sign_mask & *(uint32_t *)&y;
  // Determine the quadrant offset
  float q = (float)((~ux_s & uy_s) >> 29 | ux_s >> 30);
  // Calculate the arctangent in the first quadrant
  float bxy_a = b * x * y;
  if (bxy_a < 0.0f) bxy_a = -bxy_a;  // avoid fabs
  float num = bxy_a + y * y;
  float atan_1q = num / (x * x + bxy_a + num + 0.000001f);
  // Translate it to the proper quadrant
  uint32_t uatan_2q = (ux_s ^ uy_s) | *(uint32_t *)&atan_1q;
  return q + *(float *)&uatan_2q;
}

#endif  // ZELDA3_MATH_UTIL_H_
