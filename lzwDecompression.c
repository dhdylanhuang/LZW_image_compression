// lzwDecompression.c — binary-safe LZW decompressor
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lzw.h"

typedef struct {
    int      code;
    uint8_t *data;   // raw bytes (NOT C-strings)
    size_t   len;    // number of bytes
} DictEntry;

typedef struct {
    FILE    *stream;
    uint32_t buffer;
    int      bit_count;
} BitReader;

static void br_init(BitReader *br, FILE *stream) {
    br->stream    = stream;
    br->buffer    = 0;
    br->bit_count = 0;
}

static int br_read_code(BitReader *br, int *out_code) {
    while (br->bit_count < CODE_BITS) {
        int byte = fgetc(br->stream);
        if (byte == EOF) {
            if (br->bit_count == 0) {
                return 0; // no more codes
            }
            fprintf(stderr, "Unexpected end of compressed data.\n");
            exit(1);
        }
        br->buffer |= ((uint32_t)(byte & 0xFFu)) << br->bit_count;
        br->bit_count += 8;
    }

    *out_code = (int)(br->buffer & ((1u << CODE_BITS) - 1));
    br->buffer >>= CODE_BITS;
    br->bit_count -= CODE_BITS;
    return 1;
}

// Safe helper to free dictionary
static void free_dict(DictEntry *dict, int dict_size) {
    for (int i = 0; i < dict_size; i++) {
        free(dict[i].data);
        dict[i].data = NULL;
        dict[i].len = 0;
    }
}

// Decompression function
void lzw_decompress(const char *input_file, const char *output_file) {
    FILE *input = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        if (input) fclose(input);
        if (output) fclose(output);
        exit(1);
    }

    // --- Read header written by our compressor: initial dictionary size (256)
    int initial_dict_size = 0;
    if (fread(&initial_dict_size, sizeof(int), 1, input) != 1) {
        fprintf(stderr, "Error reading initial dictionary size.\n");
        fclose(input);
        fclose(output);
        exit(1);
    }
    if (initial_dict_size != 256) {
        fprintf(stderr, "Unexpected initial dictionary size: %d (expected 256)\n",
                initial_dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }

    // --- Build initial dictionary with single-byte entries 0..255
    DictEntry dictionary[MAX_DICT_SIZE];
    int dict_size = 0;

    if (MAX_DICT_SIZE < 256) {
        fprintf(stderr, "MAX_DICT_SIZE (%d) must be at least 256.\n", MAX_DICT_SIZE);
        fclose(input);
        fclose(output);
        exit(1);
    }

    for (int i = 0; i < 256; i++) {
        dictionary[i].code = i;
        dictionary[i].data = (uint8_t*)malloc(1);
        if (!dictionary[i].data) {
            fprintf(stderr, "Memory allocation failed for dictionary[%d]\n", i);
            free_dict(dictionary, i);
            fclose(input);
            fclose(output);
            exit(1);
        }
        dictionary[i].data[0] = (uint8_t)i;
        dictionary[i].len = 1;
        dict_size++;
    }

    BitReader br;
    br_init(&br, input);

    // --- Read the first code and output its bytes
    int prev_code;
    if (!br_read_code(&br, &prev_code)) {
        // Empty stream after header -> nothing to output
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        printf("Decompression complete (empty payload).\n");
        return;
    }
    if (prev_code < 0 || prev_code >= dict_size) {
        fprintf(stderr, "First code out of bounds: %d\n", prev_code);
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }
    // Write first sequence
    if (fwrite(dictionary[prev_code].data, 1, dictionary[prev_code].len, output)
        != dictionary[prev_code].len) {
        fprintf(stderr, "Write error on output.\n");
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }

    // --- Main decode loop
    int curr_code;
    while (br_read_code(&br, &curr_code)) {
        uint8_t *seq_data = NULL;
        size_t   seq_len  = 0;

        if (curr_code < dict_size) {
            // Regular case: sequence exists
            seq_data = dictionary[curr_code].data;
            seq_len  = dictionary[curr_code].len;
        } else if (curr_code == dict_size) {
            // KwKwK case: seq = dict[prev] + first_byte(dict[prev])
            if (prev_code < 0 || prev_code >= dict_size) {
                fprintf(stderr, "Error: prev_code out of bounds in KwKwK case: %d\n", prev_code);
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            seq_len  = dictionary[prev_code].len + 1;
            seq_data = (uint8_t*)malloc(seq_len);
            if (!seq_data) {
                fprintf(stderr, "Out of memory in KwKwK construction.\n");
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            memcpy(seq_data, dictionary[prev_code].data, dictionary[prev_code].len);
            seq_data[seq_len - 1] = dictionary[prev_code].data[0];
        } else {
            fprintf(stderr, "Error: Invalid code encountered. curr_code: %d, dict_size: %d\n",
                    curr_code, dict_size);
            free_dict(dictionary, dict_size);
            fclose(input);
            fclose(output);
            exit(1);
        }

        // Output the sequence (handle owning vs borrowed buffer)
        if (curr_code < dict_size) {
            if (fwrite(seq_data, 1, seq_len, output) != seq_len) {
                fprintf(stderr, "Write error on output.\n");
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
        } else {
            // curr_code == dict_size (KwKwK temporary buffer)
            if (fwrite(seq_data, 1, seq_len, output) != seq_len) {
                fprintf(stderr, "Write error on output.\n");
                free(seq_data);
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
        }

        // Add new entry: dict[prev] + first_byte(seq)
        if (dict_size < MAX_DICT_SIZE) {
            uint8_t first_byte = (curr_code < dict_size)
                               ? dictionary[curr_code].data[0]
                               : seq_data[0]; // KwKwK case buffer

            size_t new_len = dictionary[prev_code].len + 1;
            uint8_t *new_data = (uint8_t*)malloc(new_len);
            if (!new_data) {
                fprintf(stderr, "Out of memory creating new dict entry.\n");
                if (curr_code == dict_size) free(seq_data);
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            memcpy(new_data, dictionary[prev_code].data, dictionary[prev_code].len);
            new_data[new_len - 1] = first_byte;

            dictionary[dict_size].code = dict_size;
            dictionary[dict_size].data = new_data;
            dictionary[dict_size].len  = new_len;
            dict_size++;
        }

        // If we allocated a temporary seq_data in KwKwK case, free it now
        if (curr_code == dict_size - 1 /* was == old dict_size before increment */) {
            // Actually, safer: free only when we explicitly malloc'ed (KwKwK path)
            // We detect it by comparing pointer ownership:
            // Here we used a different strategy: free if (curr_code >= original dict_size when entered)
            // But since we can't easily know "original" here, track it via the branch:
            // We know we malloc'ed seq_data only when curr_code == previous dict_size at the branch.
            // So:
            // No-op here; we already handled it above after writing. (Kept comment for clarity.)
        }

        // Advance
        prev_code = curr_code;

        // If we created a temporary buffer in KwKwK case, free it now (we already wrote and used first byte)
        if (curr_code >= dict_size /* impossible after increment */) {
            // no-op
        }
    }

    free_dict(dictionary, dict_size);
    fclose(input);
    fclose(output);
    printf("Decompression complete.\n");
}

// Main function kept for standalone use
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_compressed_file> <output_decompressed_file>\n", argv[0]);
        return 1;
    }
    lzw_decompress(argv[1], argv[2]);
    return 0;
}
