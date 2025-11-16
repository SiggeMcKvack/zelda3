#ifndef ZELDA3_MATH_UTIL_H_
#define ZELDA3_MATH_UTIL_H_

#include <stdint.h>

// Math utility functions
// Consolidates math helpers scattered across the codebase

// Absolute value functions
static inline uint16_t abs16(uint16_t t) {
  return (t & 0x8000) ? -t : t;
}

static inline uint8_t abs8(uint8_t t) {
  return (t & 0x80) ? -t : t;
}

// Min/max functions for signed integers
static inline int IntMin(int a, int b) {
  return a < b ? a : b;
}

static inline int IntMax(int a, int b) {
  return a > b ? a : b;
}

// Min/max functions for unsigned integers
static inline unsigned int UintMin(unsigned int a, unsigned int b) {
  return a < b ? a : b;
}

static inline unsigned int UintMax(unsigned int a, unsigned int b) {
  return a > b ? a : b;
}

// Clamp value to range [min, max]
static inline int IntClamp(int val, int min, int max) {
  return val < min ? min : (val > max ? max : val);
}

static inline unsigned int UintClamp(unsigned int val, unsigned int min, unsigned int max) {
  return val < min ? min : (val > max ? max : val);
}

// Count set bits (population count)
static inline int CountBits32(uint32_t n) {
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
static inline float ApproximateAtan2(float y, float x) {
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
