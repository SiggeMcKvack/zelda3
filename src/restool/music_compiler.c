// music_compiler.c - Pure C music compiler (replaces Python compile_music.py)
// Port of assets/compile_music.py to C

#include "music_compiler.h"
#include "yaml_util.h"
#include "asset_reader.h"
#include "../logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Constants
// ============================================================================

// Note names to indices (72 notes + 2 special)
static const char *kKeys[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

// Effect names (27 total) - order matters, index is the effect code
static const char *kEffectNames[] = {
    "Instrument", "Pan", "PanFade", "Vibrato", "VibratoOff",
    "SongVolume", "SongVolumeFade", "Tempo", "TempoFade",
    "Transpose", "ChannelTranpose", "Tremolo", "TremoloOff",
    "Volume", "VolumeFade", "Call", "VibratoFade",
    "PitchEnvelopeTo", "PitchEnvelopeFrom", "PitchEnvelopeOff",
    "FineTune", "EchoEnable", "EchoOff", "EchoSetup", "EchoVolumeFade",
    "PitchSlide", "PercussionDefine"
};
#define NUM_EFFECTS 27

// Effect argument byte counts
static const int kEffectByteLength[] = {
    1, 1, 2, 3, 0, 1, 2, 1, 2, 1, 1, 3, 0, 1, 2, 3, 1, 3, 3, 0, 1, 3, 0, 3, 3, 3, 1
};

// Special addresses where gaps can start
static const uint16_t kGapStartAddrs[] = {0x2b00, 0x2880, 0xd000};
#define NUM_GAP_ADDRS 3

// ============================================================================
// Data Structures
// ============================================================================

typedef enum {
    SYM_NONE,
    SYM_SONG,
    SYM_PHRASE,
    SYM_PATTERN,
    SYM_SFX_PATTERN,
    SYM_SONG_LIST,
    SYM_SFX_LIST
} SymbolType;

// Forward declarations
typedef struct Symbol Symbol;
typedef struct PatternLine PatternLine;

// Symbol entry
struct Symbol {
    char *name;
    SymbolType type;
    uint16_t ea;           // Address from name (e.g., "Song_0x2880" -> 0x2880)
    uint16_t write_addr;   // Address after serialization
    bool defined;
    void *data;            // Type-specific data
    Symbol *next;          // For hash table chaining
};

// Phrase loop (special phrase reference)
typedef struct {
    int loops;
    int jmp;
} PhraseLoop;

// Phrase reference (either symbol or loop)
typedef struct {
    bool is_loop;
    union {
        Symbol *sym;
        PhraseLoop loop;
    };
} PhraseRef;

// Song data
typedef struct {
    PhraseRef *phrases;
    int phrase_count;
} Song;

// Phrase data
typedef struct {
    Symbol *patterns[8];
} Phrase;

// Pattern line (note or effect)
struct PatternLine {
    int type;              // 0=note, 1=effect, 2=call
    int note_or_effect;    // Note index (0-73) or effect index (0-26)
    int note_length;       // -1 for "--"
    int volstuff;          // -1 for "--"
    int args[4];           // Effect arguments
    int arg_count;
    Symbol *call_target;   // For Call effect
};

// Pattern data
typedef struct {
    PatternLine *lines;
    int line_count;
    bool fallthrough;
} Pattern;

// SFX pattern data
typedef struct {
    char **lines;
    int line_count;
} SfxPattern;

// Song list data
typedef struct {
    Symbol **songs;
    int song_count;
} SongList;

// SFX list data
typedef struct {
    Symbol **patterns;
    int *next;
    int *echo;
    int count;
    bool has_echo;
} SfxList;

// Symbol table (hash table)
#define SYMBOL_TABLE_SIZE 2048
typedef struct {
    Symbol *buckets[SYMBOL_TABLE_SIZE];
} SymbolTable;

// Relocation entry
typedef struct {
    uint16_t offset;
    Symbol *sym;
} Reloc;

// Serializer state
typedef struct {
    uint8_t memory[0x10000];
    bool written[0x10000];
    Reloc *relocs;
    int reloc_count;
    int reloc_capacity;
    uint16_t addr;
    bool addr_valid;
} Serializer;

// Sorted entity list for ordering
typedef struct {
    Symbol **items;
    int count;
    int capacity;
} EntityList;

// Music info from YAML
typedef struct {
    struct {
        char file[64];
        int repeat;
        bool has_repeat;
    } samples[32];
    int sample_count;

    struct {
        int sample;
        int decay;
        int attack;
        int sustain_level;
        int sustain_rate;
        int vxgain;
        int pitch_base;
    } instruments[32];
    int instrument_count;

    int note_gate_off[8];
    int note_volume[16];

    struct {
        int voll;
        int volr;
        int pitch;
        int sample;
        int decay;
        int attack;
        int sustain_level;
        int sustain_rate;
        int vxgain;
        int pitch_base;
    } sfx_instruments[32];
    int sfx_instrument_count;
} MusicInfo;

// ============================================================================
// Utility Functions
// ============================================================================

static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

// Parse note name to index (0-71 for notes, 72 for hold, 73 for rest)
// Returns -1 if not a note
static int parse_note(const char *s) {
    if (strcmp(s, "-+-") == 0) return 72;  // Hold
    if (strcmp(s, "---") == 0) return 73;  // Rest
    if (strcmp(s, ".") == 0) return -2;    // SFX continuation

    if (strlen(s) != 3) return -1;

    // Find key
    char key[3] = {s[0], s[1], 0};
    int key_idx = -1;
    for (int i = 0; i < 12; i++) {
        if (strcmp(key, kKeys[i]) == 0) {
            key_idx = i;
            break;
        }
    }
    if (key_idx < 0) return -1;

    // Get octave (1-6)
    char oct = s[2];
    if (oct < '1' || oct > '6') return -1;
    int octave = oct - '1';

    return key_idx + octave * 12;
}

// Find effect by name, returns index or -1
static int find_effect(const char *name) {
    for (int i = 0; i < NUM_EFFECTS; i++) {
        if (strcmp(name, kEffectNames[i]) == 0)
            return i;
    }
    return -1;
}

// Check if address is a gap start
static bool is_gap_start(uint16_t addr) {
    for (int i = 0; i < NUM_GAP_ADDRS; i++) {
        if (kGapStartAddrs[i] == addr)
            return true;
    }
    return false;
}

// ============================================================================
// Symbol Table Functions
// ============================================================================

static void SymbolTable_Init(SymbolTable *st) {
    memset(st->buckets, 0, sizeof(st->buckets));
}

static void SymbolTable_Free(SymbolTable *st) {
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        Symbol *s = st->buckets[i];
        while (s) {
            Symbol *next = s->next;
            free(s->name);
            if (s->data) {
                // Free type-specific data
                switch (s->type) {
                    case SYM_SONG: {
                        Song *song = (Song*)s->data;
                        free(song->phrases);
                        free(song);
                        break;
                    }
                    case SYM_PHRASE:
                        free(s->data);
                        break;
                    case SYM_PATTERN: {
                        Pattern *pat = (Pattern*)s->data;
                        free(pat->lines);
                        free(pat);
                        break;
                    }
                    case SYM_SFX_PATTERN: {
                        SfxPattern *sfx = (SfxPattern*)s->data;
                        for (int j = 0; j < sfx->line_count; j++)
                            free(sfx->lines[j]);
                        free(sfx->lines);
                        free(sfx);
                        break;
                    }
                    case SYM_SONG_LIST: {
                        SongList *sl = (SongList*)s->data;
                        free(sl->songs);
                        free(sl);
                        break;
                    }
                    case SYM_SFX_LIST: {
                        SfxList *sfl = (SfxList*)s->data;
                        free(sfl->patterns);
                        free(sfl->next);
                        free(sfl->echo);
                        free(sfl);
                        break;
                    }
                    default:
                        break;
                }
            }
            free(s);
            s = next;
        }
    }
}

// Get or create symbol
static Symbol* SymbolTable_Get(SymbolTable *st, const char *name, SymbolType type, bool is_create) {
    if (strcmp(name, "None") == 0)
        return NULL;

    uint32_t h = hash_string(name) % SYMBOL_TABLE_SIZE;

    // Look for existing
    for (Symbol *s = st->buckets[h]; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            if (is_create) {
                if (s->defined) {
                    LogError("Symbol %s already defined", name);
                    return NULL;
                }
                s->defined = true;
            }
            if (type != SYM_NONE && s->type != SYM_NONE && s->type != type) {
                LogError("Symbol %s type mismatch", name);
                return NULL;
            }
            if (s->type == SYM_NONE)
                s->type = type;
            return s;
        }
    }

    // Create new
    Symbol *s = calloc(1, sizeof(Symbol));
    s->name = strdup(name);
    s->type = type;
    s->defined = is_create;
    s->ea = 0;
    s->write_addr = 0;
    s->data = NULL;

    // Parse address from name if present
    const char *hex = strstr(name, "_0x");
    if (hex) {
        s->ea = (uint16_t)strtol(hex + 3, NULL, 16);
    }

    // Insert into hash table
    s->next = st->buckets[h];
    st->buckets[h] = s;

    return s;
}

// ============================================================================
// Entity List Functions
// ============================================================================

static void EntityList_Init(EntityList *el) {
    el->items = NULL;
    el->count = 0;
    el->capacity = 0;
}

static void EntityList_Free(EntityList *el) {
    free(el->items);
}

static void EntityList_Add(EntityList *el, Symbol *sym) {
    if (el->count >= el->capacity) {
        el->capacity = el->capacity ? el->capacity * 2 : 64;
        el->items = realloc(el->items, el->capacity * sizeof(Symbol*));
    }
    el->items[el->count++] = sym;
}

// Compare function for sorting by address
static int compare_by_ea(const void *a, const void *b) {
    Symbol *sa = *(Symbol**)a;
    Symbol *sb = *(Symbol**)b;
    return (int)sa->ea - (int)sb->ea;
}

// ============================================================================
// Serializer Functions
// ============================================================================

static void Serializer_Init(Serializer *s) {
    memset(s->memory, 0, sizeof(s->memory));
    memset(s->written, 0, sizeof(s->written));
    s->relocs = NULL;
    s->reloc_count = 0;
    s->reloc_capacity = 0;
    s->addr = 0;
    s->addr_valid = false;
}

static void Serializer_Free(Serializer *s) {
    free(s->relocs);
}

static void Serializer_WriteByte(Serializer *s, uint8_t b) {
    if (s->written[s->addr]) {
        LogError("Memory conflict at 0x%04x", s->addr);
    }
    s->memory[s->addr] = b;
    s->written[s->addr] = true;
    s->addr++;
}

static void Serializer_WriteAt(Serializer *s, uint16_t addr, const uint8_t *data, int len) {
    for (int i = 0; i < len; i++) {
        s->memory[addr + i] = data[i];
        s->written[addr + i] = true;
    }
}

static void Serializer_WriteWordAt(Serializer *s, uint16_t addr, uint16_t val) {
    s->memory[addr] = val & 0xff;
    s->memory[addr + 1] = (val >> 8) & 0xff;
    s->written[addr] = true;
    s->written[addr + 1] = true;
}

static void Serializer_WriteReloc(Serializer *s, Symbol *sym) {
    // Write placeholder
    Serializer_WriteByte(s, 0);
    Serializer_WriteByte(s, 0);

    // Add reloc if symbol is not NULL
    if (sym) {
        if (s->reloc_count >= s->reloc_capacity) {
            s->reloc_capacity = s->reloc_capacity ? s->reloc_capacity * 2 : 256;
            s->relocs = realloc(s->relocs, s->reloc_capacity * sizeof(Reloc));
        }
        s->relocs[s->reloc_count].offset = s->addr - 2;
        s->relocs[s->reloc_count].sym = sym;
        s->reloc_count++;
    }
}

static void Serializer_ProcessRelocs(Serializer *s) {
    for (int i = 0; i < s->reloc_count; i++) {
        uint16_t off = s->relocs[i].offset;
        Symbol *sym = s->relocs[i].sym;
        s->memory[off] = sym->write_addr & 0xff;
        s->memory[off + 1] = (sym->write_addr >> 8) & 0xff;
    }
}

// ============================================================================
// Writers for different types
// ============================================================================

static void Serializer_WriteSong(Serializer *s, Song *song) {
    for (int i = 0; i < song->phrase_count; i++) {
        PhraseRef *pr = &song->phrases[i];
        if (pr->is_loop) {
            // PhraseLoop: write [loops, 0, addr_lo, addr_hi]
            int target = s->addr + pr->loop.jmp * 2;
            Serializer_WriteByte(s, pr->loop.loops);
            Serializer_WriteByte(s, 0);
            Serializer_WriteByte(s, target & 0xff);
            Serializer_WriteByte(s, (target >> 8) & 0xff);
        } else {
            Serializer_WriteReloc(s, pr->sym);
        }
    }
    // Terminator
    Serializer_WriteByte(s, 0);
    Serializer_WriteByte(s, 0);
}

static void Serializer_WritePhrase(Serializer *s, Phrase *phrase) {
    for (int i = 0; i < 8; i++) {
        Serializer_WriteReloc(s, phrase->patterns[i]);
    }
}

static void Serializer_WritePattern(Serializer *s, Pattern *pat) {
    for (int i = 0; i < pat->line_count; i++) {
        PatternLine *pl = &pat->lines[i];

        if (pl->type == 0) {
            // Note
            if (pl->note_length >= 0)
                Serializer_WriteByte(s, (uint8_t)pl->note_length);
            if (pl->volstuff >= 0)
                Serializer_WriteByte(s, (uint8_t)pl->volstuff);
            Serializer_WriteByte(s, 0x80 | pl->note_or_effect);
        } else if (pl->type == 1) {
            // Effect
            Serializer_WriteByte(s, 0xe0 + pl->note_or_effect);
            for (int j = 0; j < pl->arg_count; j++)
                Serializer_WriteByte(s, (uint8_t)pl->args[j]);
        } else if (pl->type == 2) {
            // Call
            Serializer_WriteByte(s, 0xef);
            // Write reloc for target address
            if (s->reloc_count >= s->reloc_capacity) {
                s->reloc_capacity = s->reloc_capacity ? s->reloc_capacity * 2 : 256;
                s->relocs = realloc(s->relocs, s->reloc_capacity * sizeof(Reloc));
            }
            Serializer_WriteByte(s, 0);
            Serializer_WriteByte(s, 0);
            s->relocs[s->reloc_count].offset = s->addr - 2;
            s->relocs[s->reloc_count].sym = pl->call_target;
            s->reloc_count++;
            // Write countdown
            Serializer_WriteByte(s, (uint8_t)pl->args[0]);
        }
    }

    // Terminator (unless fallthrough)
    if (!pat->fallthrough)
        Serializer_WriteByte(s, 0);
}

static void Serializer_WriteSfxPattern(Serializer *s, SfxPattern *sfx) {
    for (int i = 0; i < sfx->line_count; i++) {
        const char *line = sfx->lines[i];

        // Tokenize line
        char buf[256];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        char *tokens[16];
        int token_count = 0;
        char *tok = strtok(buf, " \t");
        while (tok && token_count < 16) {
            tokens[token_count++] = tok;
            tok = strtok(NULL, " \t");
        }
        if (token_count == 0) continue;

        const char *cmd = tokens[0];

        if (strcmp(cmd, "SetInstrument") == 0) {
            Serializer_WriteByte(s, 0xe0);
            Serializer_WriteByte(s, (uint8_t)atoi(tokens[1]));
        } else if (strcmp(cmd, "Restart") == 0) {
            Serializer_WriteByte(s, 0xff);
            return;
        } else if (strcmp(cmd, "Fallthrough") == 0) {
            return;
        } else {
            // Note or continuation
            int note = parse_note(cmd);
            if (note >= -2) {
                // token[1] = length/duration, token[2] = vol1, token[3] = vol2
                // token[4] = "PitchSlide" (optional), token[5,6,7] = pitchslide args

                if (token_count > 1 && strcmp(tokens[1], "--") != 0)
                    Serializer_WriteByte(s, (uint8_t)atoi(tokens[1]));
                if (token_count > 2 && strcmp(tokens[2], "---") != 0)
                    Serializer_WriteByte(s, (uint8_t)atoi(tokens[2]));
                if (token_count > 3 && strcmp(tokens[3], "---") != 0)
                    Serializer_WriteByte(s, (uint8_t)atoi(tokens[3]));

                if (token_count >= 8 && strcmp(tokens[4], "PitchSlide") == 0) {
                    if (note == -2) {
                        // Continuation with pitch slide
                        Serializer_WriteByte(s, 0xf1);
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[5]));
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[6]));
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[7]));
                    } else {
                        // Note with pitch slide
                        Serializer_WriteByte(s, 0xf9);
                        Serializer_WriteByte(s, (uint8_t)(note | 0x80));
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[5]));
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[6]));
                        Serializer_WriteByte(s, (uint8_t)atoi(tokens[7]));
                    }
                } else {
                    // Just the note
                    if (note >= 0)
                        Serializer_WriteByte(s, (uint8_t)(note | 0x80));
                }
            }
        }
    }

    // Terminator
    Serializer_WriteByte(s, 0);
}

static void Serializer_WriteSfxList(Serializer *s, SfxList *list) {
    // Write pattern relocs
    for (int i = 0; i < list->count; i++) {
        Serializer_WriteReloc(s, list->patterns[i]);
    }
    // Write next values
    for (int i = 0; i < list->count; i++) {
        Serializer_WriteByte(s, (uint8_t)list->next[i]);
    }
    // Write echo values if present
    if (list->has_echo) {
        for (int i = 0; i < list->count; i++) {
            Serializer_WriteByte(s, (uint8_t)list->echo[i]);
        }
    }
}

static void Serializer_WriteSongList(Serializer *s, SongList *list) {
    for (int i = 0; i < list->song_count; i++) {
        Serializer_WriteReloc(s, list->songs[i]);
    }
}

static void Serializer_WriteObj(Serializer *s, Symbol *sym) {
    // Set address
    if (sym->ea != 0) {
        if (!s->addr_valid || is_gap_start(sym->ea)) {
            s->addr = sym->ea;
            s->addr_valid = true;
        } else if (sym->ea != s->addr) {
            LogError("Address mismatch for %s: 0x%x != 0x%x", sym->name, sym->ea, s->addr);
        }
    }

    sym->write_addr = s->addr;

    switch (sym->type) {
        case SYM_SONG:
            Serializer_WriteSong(s, (Song*)sym->data);
            break;
        case SYM_PHRASE:
            Serializer_WritePhrase(s, (Phrase*)sym->data);
            break;
        case SYM_PATTERN:
            Serializer_WritePattern(s, (Pattern*)sym->data);
            break;
        case SYM_SFX_PATTERN:
            Serializer_WriteSfxPattern(s, (SfxPattern*)sym->data);
            break;
        case SYM_SFX_LIST:
            Serializer_WriteSfxList(s, (SfxList*)sym->data);
            break;
        case SYM_SONG_LIST:
            Serializer_WriteSongList(s, (SongList*)sym->data);
            break;
        default:
            LogError("Unknown symbol type for %s", sym->name);
            break;
    }
}

// ============================================================================
// Text File Parser
// ============================================================================

// Trim whitespace from end of string
static void trim_end(char *s) {
    int len = strlen(s);
    while (len > 0 && isspace(s[len - 1]))
        s[--len] = 0;
}

// Parse song section
static bool parse_song(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    Symbol *sym = SymbolTable_Get(st, name, SYM_SONG, true);
    if (!sym) return false;

    Song *song = calloc(1, sizeof(Song));
    song->phrases = calloc(line_count, sizeof(PhraseRef));
    song->phrase_count = line_count;

    for (int i = 0; i < line_count; i++) {
        char *line = lines[i];
        trim_end(line);

        // Check for PhraseLoop
        if (strncmp(line, "PhraseLoop ", 11) == 0) {
            int loops, jmp;
            if (sscanf(line + 11, "%d %d", &loops, &jmp) == 2) {
                song->phrases[i].is_loop = true;
                song->phrases[i].loop.loops = loops;
                song->phrases[i].loop.jmp = jmp;
            } else {
                LogError("Invalid PhraseLoop: %s", line);
                free(song->phrases);
                free(song);
                return false;
            }
        } else {
            // Regular phrase reference
            song->phrases[i].is_loop = false;
            song->phrases[i].sym = SymbolTable_Get(st, line, SYM_PHRASE, false);
        }
    }

    sym->data = song;
    EntityList_Add(el, sym);
    return true;
}

// Parse phrase section
static bool parse_phrase(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    if (line_count != 8) {
        LogError("Phrase %s must have 8 lines, got %d", name, line_count);
        return false;
    }

    Symbol *sym = SymbolTable_Get(st, name, SYM_PHRASE, true);
    if (!sym) return false;

    Phrase *phrase = calloc(1, sizeof(Phrase));
    for (int i = 0; i < 8; i++) {
        trim_end(lines[i]);
        phrase->patterns[i] = SymbolTable_Get(st, lines[i], SYM_PATTERN, false);
    }

    sym->data = phrase;
    EntityList_Add(el, sym);
    return true;
}

// Parse pattern section
static bool parse_pattern(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    Symbol *sym = SymbolTable_Get(st, name, SYM_PATTERN, true);
    if (!sym) return false;

    Pattern *pat = calloc(1, sizeof(Pattern));
    pat->lines = calloc(line_count, sizeof(PatternLine));
    pat->line_count = 0;
    pat->fallthrough = false;

    for (int i = 0; i < line_count; i++) {
        char *line = lines[i];
        trim_end(line);
        if (strlen(line) == 0) continue;

        // Tokenize
        char buf[256];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        char *tokens[16];
        int token_count = 0;
        char *tok = strtok(buf, " \t");
        while (tok && token_count < 16) {
            tokens[token_count++] = tok;
            tok = strtok(NULL, " \t");
        }
        if (token_count == 0) continue;

        const char *cmd = tokens[0];

        if (strcmp(cmd, "Fallthrough") == 0) {
            pat->fallthrough = true;
        } else if (strcmp(cmd, "Call") == 0 && token_count >= 3) {
            PatternLine *pl = &pat->lines[pat->line_count++];
            pl->type = 2;  // Call
            pl->call_target = SymbolTable_Get(st, tokens[1], SYM_PATTERN, false);
            pl->args[0] = atoi(tokens[2]);
            pl->arg_count = 1;
        } else {
            int effect = find_effect(cmd);
            if (effect >= 0) {
                // Effect
                PatternLine *pl = &pat->lines[pat->line_count++];
                pl->type = 1;
                pl->note_or_effect = effect;
                pl->arg_count = 0;
                for (int j = 1; j < token_count && pl->arg_count < kEffectByteLength[effect]; j++) {
                    pl->args[pl->arg_count++] = atoi(tokens[j]);
                }
            } else {
                int note = parse_note(cmd);
                if (note >= 0 && note <= 73) {
                    // Note
                    PatternLine *pl = &pat->lines[pat->line_count++];
                    pl->type = 0;
                    pl->note_or_effect = note;
                    pl->note_length = -1;
                    pl->volstuff = -1;

                    if (token_count >= 2 && strcmp(tokens[1], "--") != 0) {
                        pl->note_length = atoi(tokens[1]);
                    }
                    if (token_count >= 3 && strcmp(tokens[2], "--") != 0) {
                        pl->volstuff = (int)strtol(tokens[2], NULL, 16);
                    }
                } else {
                    LogError("Unknown pattern command: %s", cmd);
                }
            }
        }
    }

    sym->data = pat;
    EntityList_Add(el, sym);
    return true;
}

// Parse SFX pattern section
static bool parse_sfx_pattern(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    Symbol *sym = SymbolTable_Get(st, name, SYM_SFX_PATTERN, true);
    if (!sym) return false;

    SfxPattern *sfx = calloc(1, sizeof(SfxPattern));
    sfx->lines = calloc(line_count, sizeof(char*));
    sfx->line_count = line_count;

    for (int i = 0; i < line_count; i++) {
        trim_end(lines[i]);
        sfx->lines[i] = strdup(lines[i]);
    }

    sym->data = sfx;
    EntityList_Add(el, sym);
    return true;
}

// Parse SFX list section
static bool parse_sfx_list(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    Symbol *sym = SymbolTable_Get(st, name, SYM_SFX_LIST, true);
    if (!sym) return false;

    SfxList *list = calloc(1, sizeof(SfxList));
    list->patterns = calloc(line_count, sizeof(Symbol*));
    list->next = calloc(line_count, sizeof(int));
    list->echo = calloc(line_count, sizeof(int));
    list->count = line_count;
    list->has_echo = false;

    for (int i = 0; i < line_count; i++) {
        char *line = lines[i];
        trim_end(line);

        // Parse: pattern_name,next[,echo]
        char pat_name[128];
        int next_val = 0, echo_val = 0;

        char *comma1 = strchr(line, ',');
        if (!comma1) {
            LogError("Invalid SFX list line: %s", line);
            continue;
        }

        int name_len = comma1 - line;
        strncpy(pat_name, line, name_len);
        pat_name[name_len] = 0;

        char *comma2 = strchr(comma1 + 1, ',');
        if (comma2) {
            next_val = atoi(comma1 + 1);
            echo_val = atoi(comma2 + 1);
            list->has_echo = true;
        } else {
            next_val = atoi(comma1 + 1);
        }

        list->patterns[i] = SymbolTable_Get(st, pat_name, SYM_SFX_PATTERN, false);
        list->next[i] = next_val;
        list->echo[i] = echo_val;
    }

    sym->data = list;
    EntityList_Add(el, sym);
    return true;
}

// Parse song list section
static bool parse_song_list(const char *name, char **lines, int line_count, SymbolTable *st, EntityList *el) {
    Symbol *sym = SymbolTable_Get(st, name, SYM_SONG_LIST, true);
    if (!sym) return false;

    SongList *list = calloc(1, sizeof(SongList));
    list->songs = calloc(line_count, sizeof(Symbol*));
    list->song_count = line_count;

    for (int i = 0; i < line_count; i++) {
        trim_end(lines[i]);
        list->songs[i] = SymbolTable_Get(st, lines[i], SYM_SONG, false);
    }

    sym->data = list;
    EntityList_Add(el, sym);
    return true;
}

// Parse a text file from memory buffer
static bool parse_text_data(const char *data, size_t size, SymbolTable *st, EntityList *el) {
    char section_name[256] = "";
    char **section_lines = NULL;
    int section_line_count = 0;
    int section_line_capacity = 0;

    const char *ptr = data;
    const char *end = data + size;
    char line[512];

    while (ptr < end) {
        // Read one line
        const char *line_end = ptr;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        size_t line_len = line_end - ptr;
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, ptr, line_len);
        line[line_len] = 0;

        // Skip newlines
        ptr = line_end;
        while (ptr < end && (*ptr == '\n' || *ptr == '\r'))
            ptr++;

        // Skip empty lines and comments
        char *p = line;
        while (*p && isspace(*p)) p++;
        if (*p == 0 || *p == '#') continue;

        if (*p == '[') {
            // Process previous section
            if (section_name[0]) {
                bool ok = true;
                if (strncmp(section_name, "Song_", 5) == 0) {
                    ok = parse_song(section_name, section_lines, section_line_count, st, el);
                } else if (strncmp(section_name, "Phrase_", 7) == 0) {
                    ok = parse_phrase(section_name, section_lines, section_line_count, st, el);
                } else if (strncmp(section_name, "Pattern_", 8) == 0) {
                    ok = parse_pattern(section_name, section_lines, section_line_count, st, el);
                } else if (strncmp(section_name, "Sfx_", 4) == 0) {
                    ok = parse_sfx_pattern(section_name, section_lines, section_line_count, st, el);
                } else if (strncmp(section_name, "SfxPort", 7) == 0) {
                    ok = parse_sfx_list(section_name, section_lines, section_line_count, st, el);
                } else if (strncmp(section_name, "SongList_", 9) == 0) {
                    ok = parse_song_list(section_name, section_lines, section_line_count, st, el);
                }

                // Free section lines
                for (int i = 0; i < section_line_count; i++)
                    free(section_lines[i]);
                section_line_count = 0;

                if (!ok) {
                    free(section_lines);
                    return false;
                }
            }

            // Parse new section name
            char *bracket_end = strchr(p, ']');
            if (bracket_end) {
                int slen = bracket_end - p - 1;
                strncpy(section_name, p + 1, slen);
                section_name[slen] = 0;
                // Strip any trailing space or arguments
                char *space = strchr(section_name, ' ');
                if (space) *space = 0;
            }
        } else {
            // Add line to current section
            if (section_line_count >= section_line_capacity) {
                section_line_capacity = section_line_capacity ? section_line_capacity * 2 : 32;
                section_lines = realloc(section_lines, section_line_capacity * sizeof(char*));
            }
            section_lines[section_line_count++] = strdup(p);
        }
    }

    // Process final section
    if (section_name[0]) {
        bool ok = true;
        if (strncmp(section_name, "Song_", 5) == 0) {
            ok = parse_song(section_name, section_lines, section_line_count, st, el);
        } else if (strncmp(section_name, "Phrase_", 7) == 0) {
            ok = parse_phrase(section_name, section_lines, section_line_count, st, el);
        } else if (strncmp(section_name, "Pattern_", 8) == 0) {
            ok = parse_pattern(section_name, section_lines, section_line_count, st, el);
        } else if (strncmp(section_name, "Sfx_", 4) == 0) {
            ok = parse_sfx_pattern(section_name, section_lines, section_line_count, st, el);
        } else if (strncmp(section_name, "SfxPort", 7) == 0) {
            ok = parse_sfx_list(section_name, section_lines, section_line_count, st, el);
        } else if (strncmp(section_name, "SongList_", 9) == 0) {
            ok = parse_song_list(section_name, section_lines, section_line_count, st, el);
        }

        for (int i = 0; i < section_line_count; i++)
            free(section_lines[i]);

        if (!ok) {
            free(section_lines);
            return false;
        }
    }

    free(section_lines);
    return true;
}

// Parse a text file (wrapper that loads from embedded assets or filesystem)
static bool parse_text_file(const char *path, SymbolTable *st, EntityList *el) {
    size_t size;
    uint8_t *data = AssetReader_Load(path, &size);
    if (!data) {
        LogError("Failed to load %s", path);
        return false;
    }

    bool result = parse_text_data((const char*)data, size, st, el);
    AssetReader_Free(data);
    return result;
}

// ============================================================================
// YAML Music Info Parser
// ============================================================================

static bool load_music_info(const char *path, MusicInfo *info) {
    memset(info, 0, sizeof(*info));

    // Try to load from embedded assets first
    size_t yaml_size;
    const uint8_t *yaml_data = AssetReader_GetEmbedded(path, &yaml_size);
    YamlDoc *doc;
    if (yaml_data) {
        doc = Yaml_LoadString(yaml_data, yaml_size);
    } else {
        doc = Yaml_LoadFile(path);
    }

    if (!doc) {
        LogError("Failed to load %s: %s", path, Yaml_GetLastError());
        return false;
    }

    YamlNode *root = Yaml_GetRoot(doc);

    // Parse samples
    YamlNode *samples = Yaml_GetMapping(root, "samples");
    if (samples) {
        info->sample_count = Yaml_GetSequenceLength(samples);
        for (int i = 0; i < info->sample_count && i < 32; i++) {
            YamlNode *sample = Yaml_GetSequence(samples, i);
            const char *file = Yaml_GetString(sample, "file", "");
            strncpy(info->samples[i].file, file, sizeof(info->samples[i].file) - 1);
            info->samples[i].repeat = Yaml_GetInt(sample, "repeat", 0);
            info->samples[i].has_repeat = Yaml_HasKey(sample, "repeat");
        }
    }

    // Parse instruments
    YamlNode *instruments = Yaml_GetMapping(root, "instruments");
    if (instruments) {
        info->instrument_count = Yaml_GetSequenceLength(instruments);
        for (int i = 0; i < info->instrument_count && i < 32; i++) {
            YamlNode *inst = Yaml_GetSequence(instruments, i);
            info->instruments[i].sample = Yaml_GetInt(inst, "sample", 0);
            info->instruments[i].decay = Yaml_GetInt(inst, "decay", 0);
            info->instruments[i].attack = Yaml_GetInt(inst, "attack", 0);
            info->instruments[i].sustain_level = Yaml_GetInt(inst, "sustain_level", 0);
            info->instruments[i].sustain_rate = Yaml_GetInt(inst, "sustain_rate", 0);
            info->instruments[i].vxgain = Yaml_GetInt(inst, "vxgain", 0);
            info->instruments[i].pitch_base = Yaml_GetInt(inst, "pitch_base", 0);
        }
    }

    // Parse note_gate_off
    YamlNode *gate_off = Yaml_GetMapping(root, "note_gate_off");
    if (gate_off) {
        int len = Yaml_GetSequenceLength(gate_off);
        for (int i = 0; i < len && i < 8; i++) {
            YamlNode *val = Yaml_GetSequence(gate_off, i);
            info->note_gate_off[i] = Yaml_AsInt(val);
        }
    }

    // Parse note_volume
    YamlNode *vol = Yaml_GetMapping(root, "note_volume");
    if (vol) {
        int len = Yaml_GetSequenceLength(vol);
        for (int i = 0; i < len && i < 16; i++) {
            YamlNode *val = Yaml_GetSequence(vol, i);
            info->note_volume[i] = Yaml_AsInt(val);
        }
    }

    // Parse sfx_instruments
    YamlNode *sfx_inst = Yaml_GetMapping(root, "sfx_instruments");
    if (sfx_inst) {
        info->sfx_instrument_count = Yaml_GetSequenceLength(sfx_inst);
        for (int i = 0; i < info->sfx_instrument_count && i < 32; i++) {
            YamlNode *inst = Yaml_GetSequence(sfx_inst, i);
            info->sfx_instruments[i].voll = Yaml_GetInt(inst, "voll", 0);
            info->sfx_instruments[i].volr = Yaml_GetInt(inst, "volr", 0);
            info->sfx_instruments[i].pitch = Yaml_GetInt(inst, "pitch", 0);
            info->sfx_instruments[i].sample = Yaml_GetInt(inst, "sample", 0);
            info->sfx_instruments[i].decay = Yaml_GetInt(inst, "decay", 0);
            info->sfx_instruments[i].attack = Yaml_GetInt(inst, "attack", 0);
            info->sfx_instruments[i].sustain_level = Yaml_GetInt(inst, "sustain_level", 0);
            info->sfx_instruments[i].sustain_rate = Yaml_GetInt(inst, "sustain_rate", 0);
            info->sfx_instruments[i].vxgain = Yaml_GetInt(inst, "vxgain", 0);
            info->sfx_instruments[i].pitch_base = Yaml_GetInt(inst, "pitch_base", 0);
        }
    }

    Yaml_Free(doc);
    return true;
}

// ============================================================================
// BRR Sample Loading and Intro Data Writing
// ============================================================================

static bool write_intro_data(Serializer *s, MusicInfo *info, const char *assets_path) {
    // Track which samples we've already loaded (for deduplication)
    struct { char file[64]; uint16_t addr; } loaded_samples[32];
    int loaded_count = 0;

    s->addr = 0x4000;
    s->addr_valid = true;

    // Load and write BRR samples
    for (int i = 0; i < info->sample_count; i++) {
        const char *file = info->samples[i].file;

        // Check if already loaded
        uint16_t addr = 0;
        for (int j = 0; j < loaded_count; j++) {
            if (strcmp(loaded_samples[j].file, file) == 0) {
                addr = loaded_samples[j].addr;
                break;
            }
        }

        if (addr == 0) {
            // Load new sample from embedded assets or filesystem
            char brr_path[512];
            snprintf(brr_path, sizeof(brr_path), "%s/%s.brr", assets_path, file);

            size_t brr_size;
            uint8_t *brr_data = AssetReader_Load(brr_path, &brr_size);
            if (!brr_data) {
                LogError("Failed to load sample: %s", brr_path);
                return false;
            }

            addr = s->addr;

            // Record for deduplication
            strncpy(loaded_samples[loaded_count].file, file, sizeof(loaded_samples[loaded_count].file) - 1);
            loaded_samples[loaded_count].addr = addr;
            loaded_count++;

            // Write sample data
            for (size_t j = 0; j < brr_size; j++)
                Serializer_WriteByte(s, brr_data[j]);

            AssetReader_Free(brr_data);
        }

        // Write sample table entry at 0x3c00 + i*4
        uint16_t repeat_addr;
        if (info->samples[i].has_repeat) {
            // repeat is in PCM samples, convert to BRR address offset
            // BRR is 9 bytes per 16 samples, so repeat/16*9
            repeat_addr = addr + info->samples[i].repeat / 16 * 9;
        } else {
            repeat_addr = s->addr;  // End of sample
        }

        Serializer_WriteWordAt(s, 0x3c00 + i * 4, addr);
        Serializer_WriteWordAt(s, 0x3c00 + i * 4 + 2, repeat_addr);
    }

    // Write echo buffer refs at 0x3c64 (6 * 2 bytes = 12 bytes)
    for (int i = 0; i < 6; i++) {
        Serializer_WriteWordAt(s, 0x3c64 + i * 2, 0xffff);
    }

    // Write instrument table at 0x3d00 (25 * 6 bytes)
    for (int i = 0; i < info->instrument_count; i++) {
        uint16_t ea = 0x3d00 + i * 6;
        s->memory[ea + 0] = info->instruments[i].sample;
        s->memory[ea + 1] = 0x80 | (info->instruments[i].decay << 4) | info->instruments[i].attack;
        s->memory[ea + 2] = (info->instruments[i].sustain_level << 5) | info->instruments[i].sustain_rate;
        s->memory[ea + 3] = info->instruments[i].vxgain;
        s->memory[ea + 4] = (info->instruments[i].pitch_base >> 8) & 0xff;
        s->memory[ea + 5] = info->instruments[i].pitch_base & 0xff;
        s->written[ea + 0] = s->written[ea + 1] = s->written[ea + 2] = true;
        s->written[ea + 3] = s->written[ea + 4] = s->written[ea + 5] = true;
    }

    // Write note_gate_off at 0x3d96 (8 bytes)
    for (int i = 0; i < 8; i++) {
        s->memory[0x3d96 + i] = info->note_gate_off[i];
        s->written[0x3d96 + i] = true;
    }

    // Write note_volume at 0x3d9e (16 bytes)
    for (int i = 0; i < 16; i++) {
        s->memory[0x3d9e + i] = info->note_volume[i];
        s->written[0x3d9e + i] = true;
    }

    // Write SFX instruments at 0x3e00 (25 * 9 bytes)
    for (int i = 0; i < info->sfx_instrument_count; i++) {
        uint16_t ea = 0x3e00 + i * 9;
        s->memory[ea + 0] = info->sfx_instruments[i].voll;
        s->memory[ea + 1] = info->sfx_instruments[i].volr;
        s->memory[ea + 2] = info->sfx_instruments[i].pitch & 0xff;
        s->memory[ea + 3] = (info->sfx_instruments[i].pitch >> 8) & 0xff;
        s->memory[ea + 4] = info->sfx_instruments[i].sample;
        s->memory[ea + 5] = 0x80 | (info->sfx_instruments[i].decay << 4) | info->sfx_instruments[i].attack;
        s->memory[ea + 6] = (info->sfx_instruments[i].sustain_level << 5) | info->sfx_instruments[i].sustain_rate;
        s->memory[ea + 7] = info->sfx_instruments[i].vxgain;
        s->memory[ea + 8] = info->sfx_instruments[i].pitch_base;
        for (int j = 0; j < 9; j++)
            s->written[ea + j] = true;
    }

    return true;
}

// ============================================================================
// Output Generation
// ============================================================================

static uint8_t* produce_loadable_seq(Serializer *s, size_t *out_size) {
    // Count output size first
    size_t size = 0;
    int start = 0;
    while (start < 0x10000) {
        // Find end of contiguous written region
        int end = start;
        while (end < 0x10000 && s->written[end])
            end++;

        if (end > start) {
            // Header: len_lo, len_hi, addr_lo, addr_hi
            size += 4 + (end - start);
        }

        // Skip unwritten region
        start = end;
        while (start < 0x10000 && !s->written[start])
            start++;
    }
    size += 2;  // Terminator

    // Allocate and fill output
    uint8_t *out = malloc(size);
    if (!out) return NULL;

    size_t pos = 0;
    start = 0;
    while (start < 0x10000) {
        int end = start;
        while (end < 0x10000 && s->written[end])
            end++;

        if (end > start) {
            int len = end - start;
            out[pos++] = len & 0xff;
            out[pos++] = (len >> 8) & 0xff;
            out[pos++] = start & 0xff;
            out[pos++] = (start >> 8) & 0xff;
            memcpy(out + pos, s->memory + start, len);
            pos += len;
        }

        start = end;
        while (start < 0x10000 && !s->written[start])
            start++;
    }

    // Terminator
    out[pos++] = 0;
    out[pos++] = 0;

    *out_size = pos;
    return out;
}

// ============================================================================
// Main Entry Point
// ============================================================================

bool MusicCompiler_CompileSoundBank(const char *song_name, const char *assets_path,
                                    uint8_t **out_data, size_t *out_size) {
    SymbolTable st;
    EntityList el;
    Serializer s;
    MusicInfo info;
    bool success = false;

    SymbolTable_Init(&st);
    EntityList_Init(&el);
    Serializer_Init(&s);

    // Build path to sound file
    char txt_path[512];
    snprintf(txt_path, sizeof(txt_path), "%s/sound_%s.txt", assets_path, song_name);

    // Parse main sound file
    if (!parse_text_file(txt_path, &st, &el)) {
        goto cleanup;
    }

    // For intro, also parse SFX file and load music info
    if (strcmp(song_name, "intro") == 0) {
        char sfx_path[512];
        snprintf(sfx_path, sizeof(sfx_path), "%s/sfx.txt", assets_path);
        if (!parse_text_file(sfx_path, &st, &el)) {
            goto cleanup;
        }

        char yaml_path[512];
        snprintf(yaml_path, sizeof(yaml_path), "%s/music_info.yaml", assets_path);
        if (!load_music_info(yaml_path, &info)) {
            goto cleanup;
        }

        // Write intro data (samples, instruments, etc.)
        if (!write_intro_data(&s, &info, assets_path)) {
            goto cleanup;
        }
    }

    // Sort entities by address
    qsort(el.items, el.count, sizeof(Symbol*), compare_by_ea);

    // Serialize all entities
    s.addr_valid = false;
    for (int i = 0; i < el.count; i++) {
        Serializer_WriteObj(&s, el.items[i]);
    }

    // For indoor song, mark Song_0x2880 as defined
    if (strcmp(song_name, "indoor") == 0) {
        Symbol *song = SymbolTable_Get(&st, "Song_0x2880", SYM_SONG, false);
        if (song) {
            song->defined = true;
            song->write_addr = 0x2880;
        }
    }

    // Check all symbols are defined
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        for (Symbol *sym = st.buckets[i]; sym; sym = sym->next) {
            if (!sym->defined && sym->type != SYM_NONE) {
                LogError("Symbol %s not defined", sym->name);
                goto cleanup;
            }
        }
    }

    // Process relocations
    Serializer_ProcessRelocs(&s);

    // Generate output
    *out_data = produce_loadable_seq(&s, out_size);
    if (!*out_data) {
        LogError("Failed to allocate output buffer");
        goto cleanup;
    }

    success = true;

cleanup:
    Serializer_Free(&s);
    EntityList_Free(&el);
    SymbolTable_Free(&st);
    return success;
}
