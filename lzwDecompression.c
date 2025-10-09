#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lzw.h"

// DECOMPRESSION DICTIONARY STRUCTURE
// code: the dictionary index (for consistency checking)
// data: pointer to the actual byte sequence
// len: number of bytes in the sequence


typedef struct {
    int      code;
    uint8_t *data;
    size_t   len;
} DictEntry;

// BIT READER FOR COMPRESSED INPUT
// Bit reader structure for reading fixed-width codes
// Reads bytes from file and extracts CODE_BITS-wide codes
// Handles bit boundaries that don't align with byte boundaries
// Manages end-of-stream detection and padding

typedef struct {
    FILE    *stream;
    uint32_t buffer;
    int      bit_count;
} BitReader;

// Initialize bit reader for given input stream
static void br_init(BitReader *br, FILE *stream) {
    br->stream    = stream;
    br->buffer    = 0;
    br->bit_count = 0;
}

// Read a CODE_BITS-wide code from the bit stream
// Accumulates bytes from file until we have enough bits to extract a complete code, returns that code.
static int br_read_code(BitReader *br, int *out_code) {
    // Ensure we have enough bits for a complete code
    while (br->bit_count < CODE_BITS) {
        int byte = fgetc(br->stream);
        if (byte == EOF) {
            if (br->bit_count == 0) {
                return 0;
            }
            
            // Check if remaining bits are just padding from encoder flush
            uint32_t mask = (1u << br->bit_count) - 1u;
            if ((br->buffer & mask) == 0) {
                // All remaining bits are zero (padding) - treat as end of stream
                br->buffer = 0;
                br->bit_count = 0;
                return 0;
            }
            
            // Non-zero bits remaining - this shouldn't happen with valid data
            fprintf(stderr, "Unexpected end of compressed data.\n");
            exit(1);
        }
        
        // Add the new byte to our bit buffer
        br->buffer |= ((uint32_t)(byte & 0xFFu)) << br->bit_count;
        br->bit_count += 8;
    }

    // IMPORTANT WHEN SWITCHING TO VARIABLE CODE SIZES
    *out_code = (int)(br->buffer & ((1u << CODE_BITS) - 1));
    
    // Remove the extracted bits from the buffer
    br->buffer >>= CODE_BITS;
    br->bit_count -= CODE_BITS;
    
    return 1; // Successfully read a code
}

// Safe cleanup function to free all allocated dictionary memory, different from compression where we free one big block
static void free_dict(DictEntry *dict, int dict_size) {
    for (int i = 0; i < dict_size; i++) {
        free(dict[i].data);
        dict[i].data = NULL;   // Prevent double-free
        dict[i].len = 0;
    }
}

// DECOMPRESSION
void lzw_decompress(const char *input_file, const char *output_file) {
    FILE *input = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        if (input) fclose(input);
        if (output) fclose(output);
        exit(1);
    }

    // Read the header written by our compressor
    int initial_dict_size = 0;
    if (fread(&initial_dict_size, sizeof(int), 1, input) != 1) {
        fprintf(stderr, "Error reading initial dictionary size.\n");
        fclose(input);
        fclose(output);
        exit(1);
    }
    
    // Verify the header matches what we expect
    if (initial_dict_size != 256) {
        fprintf(stderr, "Unexpected initial dictionary size: %d (expected 256)\n",
                initial_dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }

    // Initialize the decompression dictionary
    DictEntry dictionary[MAX_DICT_SIZE];
    int dict_size = 0;

    // Verify we have enough space for the initial dictionary
    if (MAX_DICT_SIZE < INIT_DICT_SIZE) {
        fprintf(stderr, "MAX_DICT_SIZE (%d) must be at least %d.\n", MAX_DICT_SIZE, INIT_DICT_SIZE);
        fclose(input);
        fclose(output);
        exit(1);
    }

    // Build initial dictionary
    for (int i = 0; i < INIT_DICT_SIZE; i++) {
        dictionary[i].code = i;
        
        // Allocate memory for single-byte sequence
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

    // Initialize bit reader for compressed input
    BitReader br;
    br_init(&br, input);

    // Read and process the first code, handled separately, no "previous" code yet
    int prev_code;
    if (!br_read_code(&br, &prev_code)) {
        // Empty compressed stream
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        printf("Decompression complete (empty payload).\n");
        return;
    }
    
    // Validate the first code is within bounds
    if (prev_code < 0 || prev_code >= dict_size) {
        fprintf(stderr, "First code out of bounds: %d\n", prev_code);
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }
    
    // Output the sequence for the first code
    if (fwrite(dictionary[prev_code].data, 1, dictionary[prev_code].len, output)
        != dictionary[prev_code].len) {
        fprintf(stderr, "Write error on output.\n");
        free_dict(dictionary, dict_size);
        fclose(input);
        fclose(output);
        exit(1);
    }

    // Process remaining codes
    int curr_code;
    while (br_read_code(&br, &curr_code)) {
        uint8_t *seq_data = NULL;
        size_t   seq_len  = 0;

        // Determine what sequence this code represents
        if (curr_code < dict_size) {
            // Code exists in current dictionary
            seq_data = dictionary[curr_code].data;
            seq_len  = dictionary[curr_code].len;
            
        } else if (curr_code == dict_size) {
            // Code doesn't exist yet, but we can construct it
            // Happens with KWKWK like "aba" -> "abab"
            // New sequence is: previous_sequence + first_byte_of_previous_sequence
            
            if (prev_code < 0 || prev_code >= dict_size) {
                fprintf(stderr, "Error: prev_code out of bounds in KwKwK case: %d\n", prev_code);
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            
            // Construct the KwKwK sequence
            seq_len  = dictionary[prev_code].len + 1;
            seq_data = (uint8_t*)malloc(seq_len);
            if (!seq_data) {
                fprintf(stderr, "Out of memory in KwKwK construction.\n");
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            
            // Copy previous sequence and append its first byte
            memcpy(seq_data, dictionary[prev_code].data, dictionary[prev_code].len);
            seq_data[seq_len - 1] = dictionary[prev_code].data[0];
            
        } else {
            //Code is beyond what should be possible
            fprintf(stderr, "Error: Invalid code encountered. curr_code: %d, dict_size: %d\n",
                    curr_code, dict_size);
            free_dict(dictionary, dict_size);
            fclose(input);
            fclose(output);
            exit(1);
        }

        // Output the determined sequence
        // Memory management different for regular vs KwKwK cases
        if (curr_code < dict_size) {
            // Regular case: using dictionary's data, don't free
            if (fwrite(seq_data, 1, seq_len, output) != seq_len) {
                fprintf(stderr, "Write error on output.\n");
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
        } else {
            // KwKwK case: using temporary allocated buffer, free after use
            if (fwrite(seq_data, 1, seq_len, output) != seq_len) {
                fprintf(stderr, "Write error on output.\n");
                free(seq_data);
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
        }

        // Add new dictionary entry
        // New entry = previous_sequence + first_byte_of_current_sequence
        if (dict_size < MAX_DICT_SIZE) {
            // Determine the first byte of the current sequence
            uint8_t first_byte = (curr_code < dict_size)
                               ? dictionary[curr_code].data[0]   // Regular case
                               : seq_data[0];                     // KwKwK case
            
            // Create new dictionary entry
            size_t new_len = dictionary[prev_code].len + 1;
            uint8_t *new_data = (uint8_t*)malloc(new_len);
            if (!new_data) {
                fprintf(stderr, "Out of memory creating new dict entry.\n");
                if (curr_code == dict_size) free(seq_data);  // Clean up KwKwK buffer
                free_dict(dictionary, dict_size);
                fclose(input);
                fclose(output);
                exit(1);
            }
            
            // Copy previous sequence, append first byte of current sequence
            memcpy(new_data, dictionary[prev_code].data, dictionary[prev_code].len);
            new_data[new_len - 1] = first_byte;

            // Add new entry to the dictionary
            dictionary[dict_size].code = dict_size;
            dictionary[dict_size].data = new_data;
            dictionary[dict_size].len  = new_len;
            dict_size++;
        }

        // Clean up KwKwK temporary buffer if we allocated one
        if (curr_code == dict_size) {
            free(seq_data);
        }

        // Move to next iteration: current code becomes previous code
        prev_code = curr_code;
    }

    // Cleanup and finish
    free_dict(dictionary, dict_size);  // Free all allocated dictionary memory
    fclose(input);
    fclose(output);
    printf("Decompression complete.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_compressed_file> <output_decompressed_file>\n", argv[0]);
        return 1;
    }
    lzw_decompress(argv[1], argv[2]);
    return 0;
}
