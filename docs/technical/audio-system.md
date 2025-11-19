# Audio System

[Home](../index.md) > [Architecture](../architecture.md) > Audio System

## Overview

Zelda3's audio system provides two playback modes:
1. **SPC emulation:** Accurate SNES audio using SPC-700 CPU and DSP emulation
2. **MSU audio:** Modern high-quality audio tracks using Opus compression

Both systems integrate with SDL audio for cross-platform output.

## Audio Pipeline

```
Audio Frame (60Hz)
  │
  ├─ Music Selection
  │   └─ Track number from game logic
  │
  ├─ SPC Player (spc_player.c)
  │   ├─ Check if MSU audio available
  │   │   ├─ YES: Use MSU track
  │   │   └─ NO: Use SPC emulation
  │   │
  │   ├─ SPC Emulation Path
  │   │   ├─ SPC-700 CPU (spc.c)
  │   │   ├─ DSP Emulation (dsp.c)
  │   │   └─ 8-channel mixing
  │   │
  │   └─ MSU Audio Path (audio.c)
  │       ├─ Opus decoder
  │       ├─ Track streaming
  │       └─ Loop point handling
  │
  └─ SDL Audio Callback
      ├─ Mix samples (stereo 16-bit)
      ├─ Volume control
      └─ Output to device
```

## SPC-700 Emulation

The SNES audio system is a complete Sony SPC-700 CPU with DSP (Digital Signal Processor).

### SPC Player

**File:** `src/spc_player.c`

The SPC player manages SPC file playback and coordinates with the emulated SPC-700:

```c
// Initialize SPC player
void SpcPlayer_Initialize(void);

// Load SPC file
void SpcPlayer_Load(const uint8 *spc_data, size_t size);

// Generate audio samples (called from SDL callback)
void SpcPlayer_GenerateSamples(int16 *samples, size_t num_samples);

// Main playback loop
void SpcPlayer_GenerateSamples(int16 *buffer, size_t sample_count) {
  for (size_t i = 0; i < sample_count; i += 2) {
    // Execute SPC-700 CPU cycles (32000 Hz clock)
    Spc_Execute(&g_spc);

    // Run DSP to generate samples
    Dsp_Cycle(&g_dsp);

    // Output stereo samples
    buffer[i + 0] = g_dsp.output_left;   // Left channel
    buffer[i + 1] = g_dsp.output_right;  // Right channel
  }
}
```

### SPC-700 CPU

**File:** `snes/spc.c`

The SPC-700 is an 8-bit CPU with 64KB RAM that runs the audio driver code:

```c
typedef struct Spc {
  // CPU state
  uint8 a, x, y;     // Registers
  uint8 sp;          // Stack pointer
  uint16 pc;         // Program counter
  uint8 psw;         // Processor status word

  // Memory
  uint8 ram[0x10000];  // 64KB RAM

  // Timers (3× 8-bit timers)
  struct {
    uint8 target;
    uint8 counter;
    bool enabled;
  } timer[3];

  // Communication ports (CPU ↔ SPC)
  uint8 port[4];
} Spc;

// Execute one instruction
void Spc_Execute(Spc *spc) {
  uint8 opcode = spc->ram[spc->pc++];

  switch (opcode) {
    case 0xE8:  // MOV A, immediate
      spc->a = spc->ram[spc->pc++];
      break;

    case 0xC4:  // MOV addr, A
      uint16 addr = ReadWord(spc, spc->pc);
      spc->pc += 2;
      spc->ram[addr] = spc->a;
      break;

    // ... 256 opcodes total
  }
}
```

### DSP (Digital Signal Processor)

**File:** `snes/dsp.c`

The DSP processes 8 voices with ADPCM sample playback, ADSR envelopes, and effects:

```c
typedef struct Dsp {
  // Voice state (8 voices)
  struct {
    int16 sample;           // Current sample
    uint16 pitch;           // Pitch (frequency)
    uint8 volume_left;      // Left channel volume
    uint8 volume_right;     // Right channel volume
    uint16 sample_addr;     // Current sample address
    uint8 adsr[2];          // ADSR envelope
    uint8 gain;             // Gain mode

    // Internal state
    int16 envelope;         // Current envelope level
    int gaussian_offset;    // Gaussian interpolation offset
    uint8 brr_byte;         // BRR byte cache
  } voice[8];

  // Global registers
  int8 main_volume_left;
  int8 main_volume_right;
  int8 echo_volume_left;
  int8 echo_volume_right;

  // Output samples
  int16 output_left;
  int16 output_right;
} Dsp;

// Process one DSP cycle (generates one sample)
void Dsp_Cycle(Dsp *dsp) {
  int32 left = 0, right = 0;

  // Mix all 8 voices
  for (int v = 0; v < 8; v++) {
    // Decode BRR sample (ADPCM)
    int16 sample = DecodeBRRSample(&dsp->voice[v]);

    // Apply Gaussian interpolation
    sample = GaussianInterpolate(sample, dsp->voice[v].gaussian_offset);

    // Apply ADSR envelope
    sample = (sample * dsp->voice[v].envelope) >> 11;

    // Mix to output
    left += (sample * dsp->voice[v].volume_left) >> 7;
    right += (sample * dsp->voice[v].volume_right) >> 7;
  }

  // Apply main volume
  dsp->output_left = (left * dsp->main_volume_left) >> 7;
  dsp->output_right = (right * dsp->main_volume_right) >> 7;

  // Clamp to 16-bit range
  dsp->output_left = clamp(dsp->output_left, -32768, 32767);
  dsp->output_right = clamp(dsp->output_right, -32768, 32767);
}
```

### BRR Sample Format

SNES audio uses BRR (Bit Rate Reduction), a form of ADPCM compression:

```c
// Decode one BRR block (9 bytes → 16 samples)
void DecodeBRRBlock(uint8 *brr_data, int16 *samples) {
  uint8 header = brr_data[0];

  int shift = header >> 4;      // Shift amount (0-12)
  int filter = (header >> 2) & 3;  // Filter type (0-3)
  bool loop = header & 2;       // Loop flag
  bool end = header & 1;        // End flag

  // Decode 8 bytes = 16 4-bit samples
  for (int i = 0; i < 8; i++) {
    uint8 byte = brr_data[1 + i];

    // High nibble
    int sample = (int8)(byte & 0xF0) >> 4;
    samples[i * 2] = ApplyFilter(sample << shift, filter);

    // Low nibble
    sample = (int8)((byte & 0x0F) << 4) >> 4;
    samples[i * 2 + 1] = ApplyFilter(sample << shift, filter);
  }
}

// BRR filters (simple IIR filters)
int16 ApplyFilter(int sample, int filter) {
  static int16 old1 = 0, old2 = 0;
  int result = sample;

  switch (filter) {
    case 0: break;  // No filter
    case 1: result += old1 - (old1 >> 4); break;
    case 2: result += (old1 * 2) - ((old1 * 3) >> 5) - old2 + (old2 >> 4); break;
    case 3: result += (old1 * 2) - ((old1 * 13) >> 6) - old2 + ((old2 * 3) >> 4); break;
  }

  old2 = old1;
  old1 = clamp(result, -32768, 32767);
  return old1;
}
```

## MSU Audio System

**File:** `src/audio.c`

MSU (Multi-Stream Unit) audio provides CD-quality music using modern codecs.

### MSU Player Structure

```c
typedef struct MsuPlayer {
  PlatformFile *f;              // Audio file handle
  OpusDecoder *opus;            // Opus decoder

  uint32 total_samples_in_file; // Total samples
  uint32 repeat_position;       // Loop point (in samples)
  uint32 samples_until_repeat;  // Countdown to loop

  uint8 state;                  // kMsuState_*
  float volume;                 // Current volume
  float volume_target;          // Target volume (for fading)
  float volume_step;            // Volume fade rate

  int16 buffer[960 * 2];        // Decode buffer (stereo)
  uint32 buffer_size;           // Samples in buffer
  uint32 buffer_pos;            // Current position in buffer
} MsuPlayer;
```

### Track Loading

```c
static void MsuPlayer_Open(MsuPlayer *mp, int track_num, bool resume) {
  // Build file path: "music/<track>.opus"
  char path[256];
  snprintf(path, sizeof(path), "music/%d.opus", track_num);

  // Open file
  mp->f = Platform_OpenFile(path, "rb");
  if (!mp->f) {
    LogWarn("MSU audio track %d not found", track_num);
    return;  // Fall back to SPC
  }

  // Read Ogg header
  ParseOggOpusHeader(mp->f, &mp->total_samples, &mp->repeat_position);

  // Create Opus decoder
  int error;
  mp->opus = opus_decoder_create(48000, 2, &error);  // 48kHz stereo
  if (error != OPUS_OK) {
    LogError("Failed to create Opus decoder: %d", error);
    return;
  }

  mp->state = kMsuState_Playing;
  mp->volume = 1.0f;
}
```

### Sample Decoding

```c
// Generate samples for SDL callback
static void MsuPlayer_GenerateSamples(MsuPlayer *mp, int16 *output, int num_samples) {
  int samples_written = 0;

  while (samples_written < num_samples) {
    // Refill buffer if needed
    if (mp->buffer_pos >= mp->buffer_size) {
      // Read Ogg packet
      uint8 packet[1500];
      size_t packet_size = ReadOggPacket(mp->f, packet, sizeof(packet));

      // Decode with Opus
      int decoded = opus_decode(mp->opus, packet, packet_size,
                                mp->buffer, 960, 0);
      if (decoded < 0) {
        LogError("Opus decode error: %d", decoded);
        break;
      }

      mp->buffer_size = decoded;
      mp->buffer_pos = 0;
    }

    // Copy samples to output
    int to_copy = min(num_samples - samples_written,
                      mp->buffer_size - mp->buffer_pos);
    for (int i = 0; i < to_copy * 2; i += 2) {
      // Apply volume
      output[samples_written * 2 + i + 0] = mp->buffer[mp->buffer_pos * 2 + i + 0] * mp->volume;
      output[samples_written * 2 + i + 1] = mp->buffer[mp->buffer_pos * 2 + i + 1] * mp->volume;
    }

    samples_written += to_copy;
    mp->buffer_pos += to_copy;

    // Handle looping
    mp->samples_until_repeat -= to_copy;
    if (mp->samples_until_repeat <= 0) {
      SeekToLoopPoint(mp);
    }
  }

  // Update volume (for fading)
  if (mp->volume != mp->volume_target) {
    mp->volume += mp->volume_step;
    if ((mp->volume_step > 0 && mp->volume >= mp->volume_target) ||
        (mp->volume_step < 0 && mp->volume <= mp->volume_target)) {
      mp->volume = mp->volume_target;
    }
  }
}
```

### MSU Deluxe

MSU Deluxe mode provides area-specific music variants:

```c
// Remap track based on game state
static uint8 RemapMsuDeluxeTrack(MsuPlayer *mp, uint8 track) {
  if (!(mp->enabled & kMsuEnabled_MsuDeluxe))
    return track;

  // Overworld: Use area-specific tracks
  if (kIsMusicOwOrDungeon[track] == 1) {
    return kMsuDeluxe_OW_Songs[overworld_area_index];
  }

  // Dungeons: Use entrance-specific tracks
  if (kIsMusicOwOrDungeon[track] == 2) {
    return kMsuDeluxe_Entrance_Songs[which_entrance];
  }

  return track;
}

// Example: Light World overworld has 16 different area tracks
static const uint8 kMsuDeluxe_OW_Songs[] = {
  37, 37, 42, 38, 38, 38, 38, 39,  // Lost Woods, Kakariko, etc.
  37, 37, 42, 38, 38, 38, 38, 41,
  // ... 160 total mappings
};
```

## SDL Audio Integration

**File:** `src/main.c` (audio callback)

SDL provides the audio callback mechanism:

```c
// Audio specification
SDL_AudioSpec audio_spec = {
  .freq = 44100,           // Sample rate
  .format = AUDIO_S16SYS,  // 16-bit signed samples
  .channels = 2,           // Stereo
  .samples = 2048,         // Buffer size (power of 2)
  .callback = AudioCallback,
  .userdata = NULL
};

// SDL audio callback (called when SDL needs samples)
void AudioCallback(void *userdata, uint8 *stream, int len) {
  int16 *output = (int16*)stream;
  int num_samples = len / 4;  // 4 bytes per stereo sample (2×16-bit)

  // Lock audio mutex (thread safety)
  if (g_audio_mutex)
    SDL_LockMutex(g_audio_mutex);

  // Generate samples
  if (g_msu_player.state == kMsuState_Playing) {
    // MSU audio active
    MsuPlayer_GenerateSamples(&g_msu_player, output, num_samples);
  } else {
    // SPC emulation
    SpcPlayer_GenerateSamples(output, num_samples);
  }

  if (g_audio_mutex)
    SDL_UnlockMutex(g_audio_mutex);
}
```

### Thread Safety

Audio runs on a separate SDL thread, requiring synchronization:

```c
// Create audio mutex BEFORE SDL_Init (Android race condition fix)
g_audio_mutex = SDL_CreateMutex();

// Initialize SDL audio
SDL_Init(SDL_INIT_AUDIO);
SDL_OpenAudio(&audio_spec, NULL);
SDL_PauseAudio(0);  // Start playback

// Access audio state safely
void ZeldaApuLock(void) {
  if (g_audio_mutex)
    SDL_LockMutex(g_audio_mutex);
}

void ZeldaApuUnlock(void) {
  if (g_audio_mutex)
    SDL_UnlockMutex(g_audio_mutex);
}

// Usage
ZeldaApuLock();
SpcPlayer_Load(spc_data, spc_size);
ZeldaApuUnlock();
```

## Audio File Formats

### SPC Files

SPC files contain a snapshot of the SNES audio state:

```
Offset   Size   Description
0x0000   33     Header ("SNES-SPC700 Sound File Data")
0x0025   2      Version (0x001A)
0x0027   1      SPC700 PC
0x0028   1      SPC700 A
0x0029   1      SPC700 X
0x002A   1      SPC700 Y
0x002B   1      SPC700 PSW
0x002C   1      SPC700 SP
0x0100   64KB   RAM contents
0x10100  128    DSP registers
```

### MSU Audio (Opus)

MSU audio uses Ogg Opus for compression:

```
File: music/<track>.opus
Format: Ogg container, Opus codec
Sample rate: 48000 Hz
Channels: 2 (stereo)
Bitrate: Variable (typically 96-128 kbps)

Loop points stored in Ogg comment tags:
  LOOP_START=<sample>
  LOOP_END=<sample>
```

## Performance Considerations

**SPC Emulation:**
- CPU intensive (complete 8-bit CPU emulation)
- ~32000 cycles per audio frame
- DSP: 8 voices × Gaussian interpolation
- Optimized with lookup tables

**MSU Audio:**
- Hardware-accelerated Opus decoding on some platforms
- Lower CPU usage than SPC
- Higher quality (48kHz vs 32kHz)
- Larger file sizes

**Audio Latency:**
- SDL buffer: 2048 samples ÷ 44100 Hz ≈ 46ms
- Smaller buffers reduce latency but increase CPU usage
- Trade-off between responsiveness and performance

## See Also

- **[Architecture](../architecture.md)** - Overall system architecture
- **[Memory Layout](memory-layout.md)** - Audio RAM layout
- **Code References:**
  - `src/audio.c` - MSU audio implementation
  - `src/spc_player.c` - SPC player
  - `snes/spc.c` - SPC-700 CPU emulation
  - `snes/dsp.c` - DSP emulation
  - `snes/apu.c` - APU registers

**External References:**
- [SPC-700 Reference](https://wiki.superfamicom.org/spc700-reference)
- [SNES DSP Documentation](https://wiki.superfamicom.org/dsp)
- [Opus Codec](https://opus-codec.org/)
- [SDL Audio Documentation](https://wiki.libsdl.org/CategoryAudio)
