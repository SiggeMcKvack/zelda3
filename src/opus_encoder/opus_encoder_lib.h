// opus_encoder_lib.h - Library API for PCM to OPUZ encoding
// Converts MSU1 PCM files (44.1kHz stereo) to OPUZ format (Opus-encoded)
// Used by CLI tool, launcher, and Android app

#ifndef OPUS_ENCODER_LIB_H
#define OPUS_ENCODER_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Error codes
#define OPUS_ENC_OK              0
#define OPUS_ENC_ERR_FILE        1   // File I/O error
#define OPUS_ENC_ERR_FORMAT      2   // Invalid MSU1 format
#define OPUS_ENC_ERR_OPUS        3   // Opus encoder error
#define OPUS_ENC_ERR_RESAMPLE    4   // Resampling error
#define OPUS_ENC_ERR_MEMORY      5   // Memory allocation error
#define OPUS_ENC_ERR_CANCELLED   6   // Encoding cancelled by user

// Encoding options
typedef struct {
    int bitrate;           // Bitrate in bps (default: 128000)
    bool has_repeat;       // Whether track loops (determines header format)
} OpusEncoderOptions;

// Default options initializer
#define OPUS_ENCODER_OPTIONS_DEFAULT { .bitrate = 128000, .has_repeat = true }

// Progress callback: returns false to cancel encoding
// progress: 0.0 to 1.0 within current file
// user_data: User-provided context pointer
typedef bool (*OpusEncoderProgressCallback)(float progress, void *user_data);

// Extended options with progress callback
typedef struct {
    int bitrate;                          // Bitrate in bps (default: 128000)
    bool has_repeat;                      // Whether track loops
    OpusEncoderProgressCallback callback; // Progress callback (can be NULL)
    void *callback_data;                  // User data for callback
} OpusEncoderOptionsEx;

// Default extended options initializer
#define OPUS_ENCODER_OPTIONS_EX_DEFAULT { .bitrate = 128000, .has_repeat = true, .callback = NULL, .callback_data = NULL }

// Get error message string
const char *OpusEncoder_GetErrorString(int error_code);

// Encode a single MSU1 file to OPUZ format
// msu_path: Input MSU1 file (.pcm)
// opuz_path: Output OPUZ file path
// options: Encoding options (NULL for defaults)
// Returns: OPUS_ENC_OK on success, error code on failure
int OpusEncoder_EncodeFile(const char *msu_path, const char *opuz_path,
                           const OpusEncoderOptions *options);

// Track repeat table for standard MSU tracks (tracks 1-47)
// Returns true if the track should loop, false if it plays once
bool OpusEncoder_TrackHasRepeat(int track_number);

// Encode from memory buffer
// pcm_data: Raw 44.1kHz stereo int16 PCM samples (interleaved L/R)
// pcm_samples: Number of stereo sample pairs
// repeat_sample: Sample position where loop starts (ignored if !has_repeat)
// options: Encoding options (NULL for defaults)
// out_data: Output buffer (caller must free with free())
// out_size: Size of output data
// Returns: OPUS_ENC_OK on success, error code on failure
int OpusEncoder_EncodeBuffer(const int16_t *pcm_data, size_t pcm_samples,
                             size_t repeat_sample, const OpusEncoderOptions *options,
                             uint8_t **out_data, size_t *out_size);

// Encode a single MSU1 file to OPUZ format with progress callback
// msu_path: Input MSU1 file (.pcm)
// opuz_path: Output OPUZ file path
// options: Extended encoding options with callback (NULL for defaults)
// Returns: OPUS_ENC_OK on success, error code on failure
int OpusEncoder_EncodeFileEx(const char *msu_path, const char *opuz_path,
                              const OpusEncoderOptionsEx *options);

#endif // OPUS_ENCODER_LIB_H
