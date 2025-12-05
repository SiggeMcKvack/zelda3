// cli.c - PCM to OPUZ encoder command line interface

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "opus_encoder_lib.h"
#include "logging.h"

#define VERSION "1.0.0"

typedef struct {
    const char *input_path;
    const char *output_path;
    int bitrate;
    bool has_repeat;
    bool batch_mode;
    const char *batch_prefix;
    const char *batch_output_dir;
    int batch_start;
    int batch_end;
    bool verbose;
    bool help;
    bool version;
} CliArgs;

static void PrintHelp(void) {
    printf("zelda3-opusencoder - PCM to OPUZ encoder v%s\n\n", VERSION);
    printf("USAGE:\n");
    printf("  zelda3-opusencoder [OPTIONS] <input.pcm> [output.opuz]\n\n");
    printf("OPTIONS:\n");
    printf("  --bitrate <N>       Bitrate in bps (default: 128000)\n");
    printf("  --no-repeat         Track doesn't loop\n");
    printf("  --repeat            Track loops (default for tracks 1-47 based on table)\n");
    printf("  --verbose, -v       Verbose output\n");
    printf("  --help, -h          Show this help\n");
    printf("  --version           Show version\n\n");
    printf("BATCH MODE:\n");
    printf("  --batch <prefix>    Batch encode MSU tracks\n");
    printf("  --output <dir>      Output directory for batch mode (default: same as input)\n");
    printf("  --start <N>         Start track number (default: 1)\n");
    printf("  --end <N>           End track number (default: 114)\n\n");
    printf("EXAMPLES:\n");
    printf("  # Encode single file\n");
    printf("  zelda3-opusencoder track-1.pcm track-1.opuz\n\n");
    printf("  # Encode with custom bitrate\n");
    printf("  zelda3-opusencoder --bitrate 192000 track-1.pcm\n\n");
    printf("  # Batch encode MSU pack\n");
    printf("  zelda3-opusencoder --batch msu/alttp_msu- --output opuz/\n\n");
    printf("  # Batch encode specific range\n");
    printf("  zelda3-opusencoder --batch msu/track- --start 1 --end 48\n\n");
}

static void PrintVersion(void) {
    printf("zelda3-opusencoder version %s\n", VERSION);
    printf("Built: %s %s\n", __DATE__, __TIME__);
}

static bool ParseArgs(int argc, char **argv, CliArgs *args) {
    memset(args, 0, sizeof(CliArgs));
    args->bitrate = 128000;
    args->has_repeat = true;  // Default to repeat
    args->batch_start = 1;
    args->batch_end = 114;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bitrate") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --bitrate requires a value\n");
                return false;
            }
            args->bitrate = atoi(argv[++i]);
            if (args->bitrate < 6000 || args->bitrate > 512000) {
                fprintf(stderr, "Error: bitrate must be between 6000 and 512000\n");
                return false;
            }
        } else if (strcmp(argv[i], "--no-repeat") == 0) {
            args->has_repeat = false;
        } else if (strcmp(argv[i], "--repeat") == 0) {
            args->has_repeat = true;
        } else if (strcmp(argv[i], "--batch") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --batch requires a prefix path\n");
                return false;
            }
            args->batch_mode = true;
            args->batch_prefix = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --output requires a directory path\n");
                return false;
            }
            args->batch_output_dir = argv[++i];
        } else if (strcmp(argv[i], "--start") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --start requires a track number\n");
                return false;
            }
            args->batch_start = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--end") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --end requires a track number\n");
                return false;
            }
            args->batch_end = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args->verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->help = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            args->version = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            return false;
        } else {
            // Positional arguments
            if (!args->input_path) {
                args->input_path = argv[i];
            } else if (!args->output_path) {
                args->output_path = argv[i];
            } else {
                fprintf(stderr, "Error: Too many arguments\n");
                return false;
            }
        }
    }

    // Validation
    if (!args->help && !args->version) {
        if (args->batch_mode) {
            if (!args->batch_prefix) {
                fprintf(stderr, "Error: --batch requires a prefix path\n");
                return false;
            }
        } else {
            if (!args->input_path) {
                fprintf(stderr, "Error: Input file required\n");
                return false;
            }
        }
    }

    return true;
}

// Generate output path from input path (replace .pcm with .opuz)
static char *GenerateOutputPath(const char *input_path) {
    size_t len = strlen(input_path);
    char *output = malloc(len + 6);  // Extra space for .opuz + null
    if (!output) return NULL;

    strcpy(output, input_path);

    // Remove .pcm extension if present
    if (len >= 4 && strcmp(&output[len - 4], ".pcm") == 0) {
        output[len - 4] = '\0';
    }

    strcat(output, ".opuz");
    return output;
}

static int EncodeSingleFile(const CliArgs *args) {
    char *output_path = NULL;
    const char *out = args->output_path;

    if (!out) {
        output_path = GenerateOutputPath(args->input_path);
        if (!output_path) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            return 1;
        }
        out = output_path;
    }

    OpusEncoderOptions opts = {
        .bitrate = args->bitrate,
        .has_repeat = args->has_repeat
    };

    printf("Encoding: %s -> %s\n", args->input_path, out);
    if (args->verbose) {
        printf("  Bitrate: %d bps\n", opts.bitrate);
        printf("  Repeat: %s\n", opts.has_repeat ? "yes" : "no");
    }

    int result = OpusEncoder_EncodeFile(args->input_path, out, &opts);
    if (result != OPUS_ENC_OK) {
        fprintf(stderr, "Error: %s\n", OpusEncoder_GetErrorString(result));
    }

    free(output_path);
    return result == OPUS_ENC_OK ? 0 : 1;
}

static int EncodeBatch(const CliArgs *args) {
    int success_count = 0;
    int fail_count = 0;
    int skip_count = 0;

    char input_path[512];
    char output_path[512];

    printf("Batch encoding tracks %d-%d from prefix: %s\n",
           args->batch_start, args->batch_end, args->batch_prefix);

    for (int track = args->batch_start; track <= args->batch_end; track++) {
        // Build input path
        snprintf(input_path, sizeof(input_path), "%s%d.pcm", args->batch_prefix, track);

        // Check if input exists
        FILE *f = fopen(input_path, "rb");
        if (!f) {
            // Skip missing tracks silently
            skip_count++;
            continue;
        }
        fclose(f);

        // Build output path
        if (args->batch_output_dir) {
            // Extract just the filename part after the prefix
            const char *prefix_end = strrchr(args->batch_prefix, '/');
            if (!prefix_end) prefix_end = strrchr(args->batch_prefix, '\\');
            const char *basename = prefix_end ? prefix_end + 1 : args->batch_prefix;
            snprintf(output_path, sizeof(output_path), "%s/%s%d.opuz",
                     args->batch_output_dir, basename, track);
        } else {
            snprintf(output_path, sizeof(output_path), "%s%d.opuz",
                     args->batch_prefix, track);
        }

        // Determine if track has repeat
        bool has_repeat = OpusEncoder_TrackHasRepeat(track);

        OpusEncoderOptions opts = {
            .bitrate = args->bitrate,
            .has_repeat = has_repeat
        };

        if (args->verbose) {
            printf("Track %d: %s -> %s (repeat=%s)\n",
                   track, input_path, output_path, has_repeat ? "yes" : "no");
        } else {
            printf("Encoding track %d...\n", track);
        }

        int result = OpusEncoder_EncodeFile(input_path, output_path, &opts);
        if (result == OPUS_ENC_OK) {
            success_count++;
        } else {
            fprintf(stderr, "  Error: %s\n", OpusEncoder_GetErrorString(result));
            fail_count++;
        }
    }

    printf("\nBatch complete: %d encoded, %d failed, %d skipped\n",
           success_count, fail_count, skip_count);

    return fail_count > 0 ? 1 : 0;
}

int main(int argc, char **argv) {
    CliArgs args;

    InitializeLogging();

    if (!ParseArgs(argc, argv, &args)) {
        fprintf(stderr, "Use --help for usage information\n");
        return 1;
    }

    if (args.help) {
        PrintHelp();
        return 0;
    }

    if (args.version) {
        PrintVersion();
        return 0;
    }

    if (args.batch_mode) {
        return EncodeBatch(&args);
    } else {
        return EncodeSingleFile(&args);
    }
}
