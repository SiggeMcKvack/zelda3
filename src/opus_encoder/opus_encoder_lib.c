// opus_encoder_lib.c - PCM to OPUZ encoder implementation
// Converts MSU1 PCM (44.1kHz stereo) to OPUZ format (Opus-encoded at 48kHz)

#include "opus_encoder_lib.h"
#include "platform.h"
#include "logging.h"

#include <opus/opus.h>
#include <samplerate.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Constants matching Python encoder
#define OPUS_FRAME_SIZE     960       // Samples per Opus frame at 48kHz
#define OPUS_SAMPLE_RATE    48000     // Opus output sample rate
#define MSU_SAMPLE_RATE     44100     // MSU1 input sample rate
#define CHANNELS            2         // Stereo
#define DEFAULT_BITRATE     128000    // 128 kbps

// OPUZ file format constants
#define OPUZ_MAGIC          "OPUZ"
#define OPUZ_VERSION        0
#define RANGE_ENTRY_SIZE    10        // 4 + 4 + 2 bytes per range entry

// CTL for forcing CELT mode (from opuslib: opuslib.api.ctl.ctl_set(11002), 1002)
#define OPUS_SET_FORCE_MODE_REQUEST 11002
#define MODE_CELT_ONLY              1002

// Track repeat table (from encode_opus.py)
static const uint8_t kMsuTracksWithRepeat[48] = {
  1,0,1,1,1,1,1,1,0,1,0,1,1,1,1,0,
  1,1,1,0,1,1,1,1,1,1,1,1,1,0,1,1,
  1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

const char *OpusEncoder_GetErrorString(int error_code) {
    switch (error_code) {
        case OPUS_ENC_OK:         return "Success";
        case OPUS_ENC_ERR_FILE:   return "File I/O error";
        case OPUS_ENC_ERR_FORMAT: return "Invalid MSU1 format";
        case OPUS_ENC_ERR_OPUS:   return "Opus encoder error";
        case OPUS_ENC_ERR_RESAMPLE: return "Resampling error";
        case OPUS_ENC_ERR_MEMORY: return "Memory allocation error";
        default:                  return "Unknown error";
    }
}

bool OpusEncoder_TrackHasRepeat(int track_number) {
    if (track_number < 0) return false;
    if (track_number >= 48) return true;  // Tracks >= 48 always repeat
    return kMsuTracksWithRepeat[track_number] != 0;
}

// Write little-endian uint16
static void WriteU16LE(uint8_t *buf, uint16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

// Write little-endian uint32
static void WriteU32LE(uint8_t *buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

// Resample 44.1kHz to 48kHz using libsamplerate
// Returns resampled buffer (caller must free) and sets out_samples
static float *ResampleTo48k(const int16_t *pcm_data, size_t pcm_samples,
                            size_t *out_samples) {
    double ratio = (double)OPUS_SAMPLE_RATE / MSU_SAMPLE_RATE;
    size_t out_sample_count = (size_t)(pcm_samples * ratio) + 1024;  // Extra padding

    // Convert int16 to float (interleaved stereo)
    float *input_float = malloc(pcm_samples * CHANNELS * sizeof(float));
    if (!input_float) return NULL;

    for (size_t i = 0; i < pcm_samples * CHANNELS; i++) {
        input_float[i] = pcm_data[i] / 32768.0f;
    }

    // Allocate output buffer
    float *output_float = malloc(out_sample_count * CHANNELS * sizeof(float));
    if (!output_float) {
        free(input_float);
        return NULL;
    }

    // Setup resampler
    SRC_DATA src_data = {
        .data_in = input_float,
        .data_out = output_float,
        .input_frames = (long)pcm_samples,
        .output_frames = (long)out_sample_count,
        .src_ratio = ratio,
    };

    // Use SRC_SINC_BEST_QUALITY (matches Python 'sinc_best')
    int error = src_simple(&src_data, SRC_SINC_BEST_QUALITY, CHANNELS);
    free(input_float);

    if (error != 0) {
        LogError("libsamplerate error: %s", src_strerror(error));
        free(output_float);
        return NULL;
    }

    *out_samples = src_data.output_frames_gen;
    return output_float;
}

// Frame list entry for building the header
typedef struct {
    uint32_t sample_pos;    // Sample position at start of this frame
    uint32_t file_offset;   // Byte offset in output file
} FrameEntry;

int OpusEncoder_EncodeBuffer(const int16_t *pcm_data, size_t pcm_samples,
                             size_t repeat_sample, const OpusEncoderOptions *options,
                             uint8_t **out_data, size_t *out_size) {
    OpusEncoderOptions opts = options ? *options : (OpusEncoderOptions)OPUS_ENCODER_OPTIONS_DEFAULT;
    int result = OPUS_ENC_OK;
    OpusEncoder *encoder = NULL;
    float *resampled = NULL;
    float *audio_padded = NULL;
    uint8_t *encoded_data = NULL;
    FrameEntry *frame_list = NULL;

    // Resample to 48kHz
    size_t resampled_samples;
    resampled = ResampleTo48k(pcm_data, pcm_samples, &resampled_samples);
    if (!resampled) {
        result = OPUS_ENC_ERR_RESAMPLE;
        goto cleanup;
    }

    // Convert repeat position to 48kHz samples
    size_t repeat_pos_48k = (size_t)((double)repeat_sample * OPUS_SAMPLE_RATE / MSU_SAMPLE_RATE);

    // Create Opus encoder
    int opus_error;
    encoder = opus_encoder_create(OPUS_SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &opus_error);
    if (!encoder) {
        LogError("Failed to create Opus encoder: %s", opus_strerror(opus_error));
        result = OPUS_ENC_ERR_OPUS;
        goto cleanup;
    }

    // Configure encoder (matching Python settings)
    opus_encoder_ctl(encoder, OPUS_SET_VBR(1));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(opts.bitrate > 0 ? opts.bitrate : DEFAULT_BITRATE));
    opus_encoder_ctl(encoder, OPUS_SET_FORCE_MODE_REQUEST, MODE_CELT_ONLY);

    // Get encoder lookahead (preskip)
    int lookahead;
    opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&lookahead));

    // Pad audio to frame boundary (add lookahead samples at end)
    size_t padded_samples = resampled_samples + lookahead;
    size_t remainder = padded_samples % OPUS_FRAME_SIZE;
    if (remainder != 0) {
        padded_samples += OPUS_FRAME_SIZE - remainder;
    }

    audio_padded = calloc(padded_samples * CHANNELS, sizeof(float));
    if (!audio_padded) {
        result = OPUS_ENC_ERR_MEMORY;
        goto cleanup;
    }
    memcpy(audio_padded, resampled, resampled_samples * CHANNELS * sizeof(float));

    // Allocate frame list
    size_t num_frames = padded_samples / OPUS_FRAME_SIZE;
    frame_list = malloc(num_frames * sizeof(FrameEntry));
    if (!frame_list) {
        result = OPUS_ENC_ERR_MEMORY;
        goto cleanup;
    }

    // Allocate encoded data buffer (estimate: 2 bytes header + max ~500 bytes per frame)
    size_t encoded_capacity = num_frames * 512;
    encoded_data = malloc(encoded_capacity);
    if (!encoded_data) {
        result = OPUS_ENC_ERR_MEMORY;
        goto cleanup;
    }

    // Encode all frames
    size_t encoded_pos = 0;
    uint8_t frame_buf[1276];  // Max Opus frame size

    for (size_t i = 0; i < num_frames; i++) {
        // Encode frame
        int len = opus_encode_float(encoder,
                                    &audio_padded[i * OPUS_FRAME_SIZE * CHANNELS],
                                    OPUS_FRAME_SIZE,
                                    frame_buf, sizeof(frame_buf));
        if (len < 0) {
            LogError("Opus encode error: %s", opus_strerror(len));
            result = OPUS_ENC_ERR_OPUS;
            goto cleanup;
        }

        // Record frame position
        frame_list[i].sample_pos = (uint32_t)(i * OPUS_FRAME_SIZE);
        frame_list[i].file_offset = (uint32_t)encoded_pos;

        // Check if first byte is 0xfc (can be stripped per Python encoder logic)
        uint16_t code = (uint16_t)len;
        const uint8_t *frame_data = frame_buf;
        if (len > 0 && frame_buf[0] == 0xfc) {
            frame_data = &frame_buf[1];
            code = ((uint16_t)(len - 1)) | 0x8000;
        }

        // Ensure capacity
        size_t need = encoded_pos + 2 + (code & 0x7fff);
        if (need > encoded_capacity) {
            encoded_capacity = need * 2;
            uint8_t *new_buf = realloc(encoded_data, encoded_capacity);
            if (!new_buf) {
                result = OPUS_ENC_ERR_MEMORY;
                goto cleanup;
            }
            encoded_data = new_buf;
        }

        // Write frame: 2-byte size + data
        WriteU16LE(&encoded_data[encoded_pos], code);
        encoded_pos += 2;
        memcpy(&encoded_data[encoded_pos], frame_data, code & 0x7fff);
        encoded_pos += (code & 0x7fff);
    }

    // Build OPUZ header
    // Format:
    //   "OPUZ" (4 bytes)
    //   VERSION (4 bytes, value 0)
    //   Range entries (10 bytes each):
    //     file_offset (4) - where Opus packets start
    //     samples_to_play (4) - number of samples in this range
    //     preskip_flags (2) - preskip in bits 0-13, flags in 14-15
    //
    // For looping tracks: 2 ranges (intro + loop)
    // For non-looping: 1 range

    int num_ranges = opts.has_repeat ? 2 : 1;
    size_t header_size = 8 + num_ranges * RANGE_ENTRY_SIZE;
    size_t total_size = header_size + encoded_pos;

    uint8_t *output = malloc(total_size);
    if (!output) {
        result = OPUS_ENC_ERR_MEMORY;
        goto cleanup;
    }

    // Write magic and version
    memcpy(output, OPUZ_MAGIC, 4);
    WriteU32LE(&output[4], OPUZ_VERSION);

    // Build range entries
    if (opts.has_repeat) {
        // Range 0: from start to end (intro plays through)
        size_t frame0 = 0;
        uint32_t file_offs0 = (uint32_t)header_size + frame_list[frame0].file_offset;
        uint32_t samples0 = (uint32_t)resampled_samples;
        uint16_t preskip0 = (uint16_t)(lookahead + 0 - frame_list[frame0].sample_pos);
        // flags: not repeat point (0x4000=0), not final (0x8000=0)
        WriteU32LE(&output[8], file_offs0);
        WriteU32LE(&output[12], samples0);
        WriteU16LE(&output[16], preskip0);

        // Range 1: from repeat point to end (loop back here)
        // Find frame containing repeat_pos_48k
        size_t frame1 = 0;
        for (size_t i = num_frames; i > 0; i--) {
            if (frame_list[i-1].sample_pos <= repeat_pos_48k) {
                frame1 = i - 1;
                break;
            }
        }
        uint32_t file_offs1 = (uint32_t)header_size + frame_list[frame1].file_offset;
        uint32_t samples1 = (uint32_t)(resampled_samples - repeat_pos_48k);
        uint16_t preskip1 = (uint16_t)(lookahead + repeat_pos_48k - frame_list[frame1].sample_pos);
        // flags: is repeat point (0x4000=1), is final (0x8000=1)
        preskip1 |= 0x4000 | 0x8000;
        WriteU32LE(&output[18], file_offs1);
        WriteU32LE(&output[22], samples1);
        WriteU16LE(&output[26], preskip1);
    } else {
        // Single range: entire track, no repeat
        size_t frame0 = 0;
        uint32_t file_offs0 = (uint32_t)header_size + frame_list[frame0].file_offset;
        uint32_t samples0 = (uint32_t)resampled_samples;
        uint16_t preskip0 = (uint16_t)(lookahead + 0 - frame_list[frame0].sample_pos);
        // flags: not repeat point, is final
        preskip0 |= 0x8000;
        WriteU32LE(&output[8], file_offs0);
        WriteU32LE(&output[12], samples0);
        WriteU16LE(&output[16], preskip0);
    }

    // Copy encoded data after header
    memcpy(&output[header_size], encoded_data, encoded_pos);

    *out_data = output;
    *out_size = total_size;

cleanup:
    if (encoder) opus_encoder_destroy(encoder);
    free(resampled);
    free(audio_padded);
    free(encoded_data);
    free(frame_list);
    return result;
}

int OpusEncoder_EncodeFile(const char *msu_path, const char *opuz_path,
                           const OpusEncoderOptions *options) {
    int result = OPUS_ENC_OK;
    uint8_t *file_data = NULL;
    uint8_t *encoded = NULL;

    // Read MSU1 file
    size_t file_size;
    file_data = Platform_ReadWholeFile(msu_path, &file_size);
    if (!file_data) {
        LogError("Failed to read file: %s", msu_path);
        return OPUS_ENC_ERR_FILE;
    }

    // Validate MSU1 header
    if (file_size < 8 || memcmp(file_data, "MSU1", 4) != 0) {
        LogError("Invalid MSU1 file: %s", msu_path);
        result = OPUS_ENC_ERR_FORMAT;
        goto cleanup;
    }

    // Read repeat position (little-endian uint32 at offset 4)
    uint32_t repeat_pos = file_data[4] | (file_data[5] << 8) |
                          (file_data[6] << 16) | (file_data[7] << 24);

    // PCM data starts at offset 8
    const int16_t *pcm_data = (const int16_t *)&file_data[8];
    size_t pcm_samples = (file_size - 8) / 4;  // 4 bytes per stereo sample (2 × int16)

    // Encode
    size_t encoded_size;
    result = OpusEncoder_EncodeBuffer(pcm_data, pcm_samples, repeat_pos, options,
                                      &encoded, &encoded_size);
    if (result != OPUS_ENC_OK) {
        goto cleanup;
    }

    // Write output file
    FILE *f = fopen(opuz_path, "wb");
    if (!f) {
        LogError("Failed to create output file: %s", opuz_path);
        result = OPUS_ENC_ERR_FILE;
        goto cleanup;
    }

    if (fwrite(encoded, 1, encoded_size, f) != encoded_size) {
        LogError("Failed to write output file: %s", opuz_path);
        fclose(f);
        result = OPUS_ENC_ERR_FILE;
        goto cleanup;
    }

    fclose(f);
    LogInfo("Encoded %s -> %s (%zu bytes)", msu_path, opuz_path, encoded_size);

cleanup:
    free(file_data);
    free(encoded);
    return result;
}
