// lzw.c — binary-safe LZW compressor
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

static int find_concat_code(DictEntry *dict, int dict_size,
                            int base_code, uint8_t next_byte) {
    size_t need_len = dict[base_code].len + 1;
    for (int i = 0; i < dict_size; i++) {
        if (dict[i].len != need_len) continue;
        // Compare dict[i] against dict[base_code] + next_byte
        if (memcmp(dict[i].data, dict[base_code].data, dict[base_code].len) == 0 &&
            dict[i].data[need_len - 1] == next_byte) {
            return i;
        }
    }
    return -1;
}

void lzw_compress(const char *input_file, const char *output_file) {
    FILE *input  = fopen(input_file,  "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        if (input) fclose(input);
        if (output) fclose(output);
        exit(1);
    }

    // ---- Header (keep compatibility with your decompressor):
    // write an int indicating the initial dictionary size (256).
    int init_dict_size = INIT_DICT_SIZE;
    fwrite(&init_dict_size, sizeof(int), 1, output);

    // ---- Build initial dictionary (single-byte entries 0..255)
    DictEntry dictionary[MAX_DICT_SIZE];
    int dict_size = 0;

    // Ensure we can at least host the 256 entries
    if (MAX_DICT_SIZE < init_dict_size) {
        fprintf(stderr, "MAX_DICT_SIZE (%d) is less than 256 — increase it.\n", MAX_DICT_SIZE);
        fclose(input);
        fclose(output);
        exit(1);
    }

    for (int i = 0; i < init_dict_size; i++) {
        dictionary[i].code = i;
        dictionary[i].data = (uint8_t*)malloc(1);
        if (!dictionary[i].data) {
            fprintf(stderr, "Out of memory initializing dictionary.\n");
            for (int k = 0; k < i; k++) free(dictionary[k].data);
            fclose(input);
            fclose(output);
            exit(1);
        }
        dictionary[i].data[0] = (uint8_t)i;
        dictionary[i].len = 1;
        dict_size++;
    }

    // Read first byte to seed the current code
    int first = fgetc(input);
    if (first == EOF) {
        // Empty input: nothing else to write; cleanup and exit
        for (int i = 0; i < dict_size; i++) free(dictionary[i].data);
        fclose(input);
        fclose(output);
        printf("Compression complete (empty input).\n");
        return;
    }

    int curr_code = first & 0xFF;

    // Main LZW loop
    for (;;) {
        int nxt = fgetc(input);
        if (nxt == EOF) {
            // Emit final code
            fwrite(&curr_code, sizeof(int), 1, output);
            break;
        }
        uint8_t k = (uint8_t)(nxt & 0xFF);

        // Look for W+k in dictionary
        int found = find_concat_code(dictionary, dict_size, curr_code, k);
        if (found != -1) {
            // Extend current sequence
            curr_code = found;
        } else {
            // Output code for W
            fwrite(&curr_code, sizeof(int), 1, output);

            // Add W+k to dictionary if space remains
            if (dict_size < MAX_DICT_SIZE) {
                DictEntry *e = &dictionary[dict_size];
                e->code = dict_size;
                e->len  = dictionary[curr_code].len + 1;  // Note: curr_code is still W here
                e->data = (uint8_t*)malloc(e->len);
                if (!e->data) {
                    fprintf(stderr, "Out of memory adding dictionary entry.\n");
                    // Cleanup and exit safely
                    for (int i = 0; i < dict_size; i++) free(dictionary[i].data);
                    fclose(input);
                    fclose(output);
                    exit(1);
                }
                // e = W + k
                memcpy(e->data, dictionary[curr_code].data, dictionary[curr_code].len);
                e->data[e->len - 1] = k;
                dict_size++;
            }

            // Set current sequence to the single-byte k
            curr_code = k;
        }
    }

    // Cleanup
    for (int i = 0; i < dict_size; i++) {
        free(dictionary[i].data);
    }
    fclose(input);
    fclose(output);
    printf("Compression complete.\n");
}
