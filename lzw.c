// lzw.c — LZW compressor with O(1) dictionary lookups (open-addressed hash)
// Drop-in replacement for your previous lzw.c. Keeps the same header format and CODE_BITS.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h> // For measuring compression time
#include "lzw.h"

// ----------------------------
// Encoder dictionary entry
// We store only (prefix_code, append_byte, len) to avoid large memcpy/malloc.
// code is the index in the dictionary array.
// prefix=-1 for the 256 root symbols.
// ----------------------------
typedef struct {
    int      prefix;   // previous code (sequence W)
    uint8_t  append;   // next byte k
    size_t   len;      // length of sequence represented by this code
} EncEntry;

// ----------------------------
// Hash table from (prefix, append) -> code
// Open addressing with linear probing.
// key = ((uint32_t)prefix << 8) | append
// ----------------------------
typedef struct {
    uint32_t key;
    int      code;   // -1 for empty
} Slot;

static Slot *ht = NULL;
static size_t ht_cap = 0;
static size_t ht_count = 0;

static inline uint32_t mk_key(int prefix, uint8_t append) {
    return ((uint32_t)prefix << 8) | append;
}

static void ht_init(size_t capacity) {
    ht_cap = 1;
    while (ht_cap < capacity) ht_cap <<= 1; // power of two
    ht = (Slot*)malloc(ht_cap * sizeof(Slot));
    if (!ht) { fprintf(stderr, "Out of memory (hash alloc)\n"); exit(1); }
    for (size_t i = 0; i < ht_cap; ++i) { ht[i].code = -1; ht[i].key = 0; }
    ht_count = 0;
}

static inline size_t ht_probe_start(uint32_t key) {
    // Knuth multiplicative hashing
    return (size_t)(key * 2654435761u) & (ht_cap - 1);
}

static int ht_find(uint32_t key) {
    size_t i = ht_probe_start(key);
    for (;;) {
        if (ht[i].code == -1) return -1;     // empty slot => not found
        if (ht[i].key == key) return ht[i].code;
        i = (i + 1) & (ht_cap - 1);
    }
}

static void ht_insert(uint32_t key, int code) {
    size_t i = ht_probe_start(key);
    for (;;) {
        if (ht[i].code == -1) {
            ht[i].key = key;
            ht[i].code = code;
            ++ht_count;
            return;
        }
        i = (i + 1) & (ht_cap - 1);
    }
}

// ----------------------------
// Bit writer (kept identical semantics: fixed CODE_BITS)
// ----------------------------
typedef struct {
    FILE    *stream;
    uint32_t buffer;
    int      bit_count;
} BitWriter;

static void bw_init(BitWriter *bw, FILE *stream) {
    bw->stream = stream;
    bw->buffer = 0;
    bw->bit_count = 0;
}

static void bw_write_code(BitWriter *bw, int code) {
    bw->buffer |= ((uint32_t)code & ((1u << CODE_BITS) - 1)) << bw->bit_count;
    bw->bit_count += CODE_BITS;

    while (bw->bit_count >= 8) {
        uint8_t byte = (uint8_t)(bw->buffer & 0xFFu);
        if (fwrite(&byte, 1, 1, bw->stream) != 1) {
            fprintf(stderr, "Error writing compressed data.\n");
            exit(1);
        }
        bw->buffer >>= 8;
        bw->bit_count -= 8;
    }
}

static void bw_flush(BitWriter *bw) {
    if (bw->bit_count > 0) {
        uint8_t byte = (uint8_t)(bw->buffer & 0xFFu);
        if (fwrite(&byte, 1, 1, bw->stream) != 1) {
            fprintf(stderr, "Error flushing compressed data.\n");
            exit(1);
        }
        bw->buffer = 0;
        bw->bit_count = 0;
    }
}

// ----------------------------
// Compression
// ----------------------------
void lzw_compress(const char *input_file, const char *output_file) {
    clock_t start_time = clock(); // Start time for compression

    FILE *input  = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        if (input) fclose(input);
        if (output) fclose(output);
        exit(1);
    }

    // Header: write initial dictionary size (kept for decoder compatibility)
    int init_dict_size = INIT_DICT_SIZE; // 256
    if (fwrite(&init_dict_size, sizeof(int), 1, output) != 1) {
        fprintf(stderr, "Error writing header.\n");
        fclose(input); fclose(output);
        exit(1);
    }

    // Allocate dictionary (encoder-side)
    EncEntry *dict = (EncEntry*)malloc(sizeof(EncEntry) * MAX_DICT_SIZE);
    if (!dict) { fprintf(stderr, "Out of memory (dict)\n"); fclose(input); fclose(output); exit(1); }

    // Seed first 256 entries: single bytes
    int dict_size = 0;
    int peak_dict_size = 0; // Track peak dictionary size
    if (MAX_DICT_SIZE < init_dict_size) {
        fprintf(stderr, "MAX_DICT_SIZE (%d) is less than %d.\n", MAX_DICT_SIZE, init_dict_size);
        fclose(input); fclose(output); free(dict); exit(1);
    }
    for (int i = 0; i < init_dict_size; ++i) {
        dict[i].prefix = -1;
        dict[i].append = (uint8_t)i;
        dict[i].len    = 1;
        ++dict_size;
    }

    // Hash capacity: ~2x MAX_DICT_SIZE for low load factor
    ht_init((size_t)MAX_DICT_SIZE * 2);

    // Metrics tracking
    size_t bytes_processed = 0;
    size_t codes_written = 0;
    size_t total_bits_written = 0;
    size_t hash_collisions = 0;
    size_t kwkwk_count = 0; // Count for KwKwK patterns

    BitWriter bw; bw_init(&bw, output);

    // Prime the stream with first byte
    int first = fgetc(input);
    if (first == EOF) {
        // Empty file: nothing but header and flush
        bw_flush(&bw);
        fclose(input); fclose(output); free(dict); free(ht);
        printf("Compression complete (empty input).\n");
        return;
    }
    int curr_code = first & 0xFF;
    ++bytes_processed;

    // Main loop
    int byte_in;
    while ((byte_in = fgetc(input)) != EOF) {
        ++bytes_processed;
        uint8_t k = (uint8_t)byte_in;

        // Is (curr_code + k) in dictionary?
        int found = ht_find(mk_key(curr_code, k));
        if (found != -1) {
            // Extend W -> W+k
            curr_code = found;
        } else {
            // Emit W
            bw_write_code(&bw, curr_code);
            ++codes_written;
            total_bits_written += CODE_BITS;

            // Check for KwKwK pattern
            if (dict[curr_code].prefix != -1 && dict[dict[curr_code].prefix].prefix != -1) {
                if (dict[dict[curr_code].prefix].append == dict[curr_code].append) {
                    ++kwkwk_count;
                }
            }

            // Add W+k if space remains
            if (dict_size < MAX_DICT_SIZE) {
                dict[dict_size].prefix = curr_code;
                dict[dict_size].append = k;
                dict[dict_size].len    = dict[curr_code].len + 1;
                // Insert into hash
                size_t start_count = ht_count;
                ht_insert(mk_key(curr_code, k), dict_size);
                if (ht_count > start_count) {
                    ++hash_collisions;
                }
                ++dict_size;
                if (dict_size > peak_dict_size) {
                    peak_dict_size = dict_size;
                }
            }
            // Start new W as single-byte k
            curr_code = (int)k;
        }
    }

    // Emit final W
    bw_write_code(&bw, curr_code);
    ++codes_written;
    total_bits_written += CODE_BITS;
    bw_flush(&bw);

    fclose(input);
    fclose(output);
    free(dict);
    free(ht);

    clock_t end_time = clock(); // End time for compression
    double compression_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Log metrics
    printf("Compression Metrics:\n");
    printf("- Dictionary Size: %d\n", dict_size);
    printf("- Peak Dictionary Size: %d\n", peak_dict_size);
    printf("- Bytes Processed: %zu\n", bytes_processed);
    printf("- Compression Time: %.2f seconds\n", compression_time);
    printf("- Number of Codes Written: %zu\n", codes_written);
    printf("- Average Code Length: %.2f bits\n", (double)total_bits_written / codes_written);
    printf("- Hash Table Load Factor: %.2f\n", (double)ht_count / ht_cap);
    printf("- Number of Collisions: %zu\n", hash_collisions);
    printf("- Final Buffer State: %d bits\n", bw.bit_count);
    printf("- KwKwK Pattern Count: %zu\n", kwkwk_count);

    printf("Compression complete.\n");
}
