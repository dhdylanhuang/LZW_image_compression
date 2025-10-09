#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "lzw.h"

// DICTIONARY STRUCTURE
// Represents a sequence in the LZW dictionary
// Instead of storing the full sequence, uses a linked-list approach:
// Prefix: points to the previous sequence (W)
// Append: the byte that extends W to create W+k
// Len: total length of this sequence (for metrics)
// Saves memory by avoiding large string copies/allocations.

typedef struct {
    int      prefix;   
    uint8_t  append; 
    size_t   len;
} EncEntry;

// HASH TABLE
// O(1) dictionary lookups
// Maps (prefix_code, append_byte) -> dictionary_index
// Open addressing with linear probing for collision resolution
// Key format: ((prefix << 8) | append) creates a unique 32-bit key

typedef struct {
    uint32_t key;
    int      code;
} Slot;

// Global hash table variables
static Slot *ht = NULL;        // Hash table array
static size_t ht_cap = 0;      // Hash table capacity (always power of 2) no % can use bitwise & instead
static size_t ht_count = 0;    // Number of occupied slots

// Creates key from prefix and append byte
static inline uint32_t mk_key(int prefix, uint8_t append) {
    return ((uint32_t)prefix << 8) | append;
}

// Initialize hash table with given capacity
static void ht_init(size_t capacity) {
    ht_cap = 1;
    while (ht_cap < capacity) ht_cap <<= 1; // Ensure power of two for fast masking
    
    ht = (Slot*)malloc(ht_cap * sizeof(Slot));
    if (!ht) { 
        fprintf(stderr, "Out of memory (hash table allocation)\n"); 
        exit(1); 
    }
    
    // Initialize all slots as empty
    for (size_t i = 0; i < ht_cap; ++i) { 
        ht[i].code = -1;
        ht[i].key = 0; 
    }
    ht_count = 0;
}

// Calculate starting position for hash table probe
static inline size_t ht_probe_start(uint32_t key) {
    // Knuth multiplicative hashing - magic number provides good hash distribution
    return (size_t)(key * 2654435761u) & (ht_cap - 1);
}

// Find a key in the hash table
static int ht_find(uint32_t key) {
    size_t i = ht_probe_start(key);
    
    // Linear probe until we find the key or an empty slot
    for (;;) {
        if (ht[i].code == -1) return -1;
        if (ht[i].key == key) return ht[i].code;
        i = (i + 1) & (ht_cap - 1);
    }
}

// Insert a new key-code pair into the hash table
static void ht_insert(uint32_t key, int code) {
    size_t i = ht_probe_start(key);
    
    // Linear probe until we find an empty slot
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

// BIT WRITER
// Fixed-width codes that don't align with byte boundaries.
// Buffers bits and outputs complete bytes when ready.
// stream: output file
// buffer: accumulates bits until we have a complete byte
// bit_count: number of bits currently in buffer
typedef struct {
    FILE    *stream;
    uint32_t buffer;
    int      bit_count;
} BitWriter;

// Initialize bit writer for given output stream
static void bw_init(BitWriter *bw, FILE *stream) {
    bw->stream = stream;
    bw->buffer = 0;
    bw->bit_count = 0;
}

// Write CODE_BITS-wide code to the bit stream
static void bw_write_code(BitWriter *bw, int code) {
    bw->buffer |= ((uint32_t)code & ((1u << CODE_BITS) - 1)) << bw->bit_count;
    bw->bit_count += CODE_BITS;

    // Output complete bytes as they become available
    while (bw->bit_count >= 8) {
        uint8_t byte = (uint8_t)(bw->buffer & 0xFFu);  // Extract lowest 8 bits
        if (fwrite(&byte, 1, 1, bw->stream) != 1) {
            fprintf(stderr, "Error writing compressed data.\n");
            exit(1);
        }
        bw->buffer >>= 8;      // Remove the written byte from buffer
        bw->bit_count -= 8;    // Update bit count
    }
}

// Flush any remaining bits in the buffer to the output stream
static void bw_flush(BitWriter *bw) {
    if (bw->bit_count > 0) {
        uint8_t byte = (uint8_t)(bw->buffer & 0xFFu);  // Get remaining bits
        if (fwrite(&byte, 1, 1, bw->stream) != 1) {
            fprintf(stderr, "Error flushing compressed data.\n");
            exit(1);
        }
        bw->buffer = 0;      // Clear buffer
        bw->bit_count = 0;   // Reset bit count
    }
}

// COMPRESSION
void lzw_compress(const char *input_file, const char *output_file) {
    clock_t start_time = clock();

    FILE *input  = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        if (input) fclose(input);
        if (output) fclose(output);
        exit(1);
    }

    // Write header: initial dictionary size for decoder compatibility
    int init_dict_size = INIT_DICT_SIZE;
    if (fwrite(&init_dict_size, sizeof(int), 1, output) != 1) {
        fprintf(stderr, "Error writing header.\n");
        fclose(input); fclose(output);
        exit(1);
    }

    // Allocate memory for the dictionary
    EncEntry *dict = (EncEntry*)malloc(sizeof(EncEntry) * MAX_DICT_SIZE);
    if (!dict) { 
        fprintf(stderr, "Out of memory (dictionary allocation)\n"); 
        fclose(input); fclose(output); exit(1); 
    }

    // Initialize dictionary and metrics
    int dict_size = 0;               
    int peak_dict_size = 0;
    
    // Validate dictionary size limits
    if (MAX_DICT_SIZE < init_dict_size) {
        fprintf(stderr, "MAX_DICT_SIZE (%d) is less than %d.\n", MAX_DICT_SIZE, init_dict_size);
        fclose(input); fclose(output); free(dict); exit(1);
    }
    
    // Seed dictionary
    for (int i = 0; i < init_dict_size; ++i) {
        dict[i].prefix = -1;          
        dict[i].append = (uint8_t)i;
        dict[i].len    = 1;
        ++dict_size;
    }

    // Initialize hash table for fast dictionary lookups
    // *2 Low load factor and better performance
    ht_init((size_t)MAX_DICT_SIZE * 2);

    size_t bytes_processed = 0;    
    size_t codes_written = 0;
    size_t total_bits_written = 0;
    size_t hash_collisions = 0;
    size_t kwkwk_count = 0;

    // Initialize bit writer for compressed output
    BitWriter bw; 
    bw_init(&bw, output);

    // Start LZW compression: read first byte to initialize current sequence
    int first = fgetc(input);
    if (first == EOF) {
        bw_flush(&bw);
        fclose(input); fclose(output); free(dict); free(ht);
        printf("Compression complete (empty input).\n");
        return;
    }
    
    // Current sequence starts as single-byte code (0-128)
    int curr_code = first & 0xFF;
    ++bytes_processed;

    // Compression loop
    int byte_in;
    while ((byte_in = fgetc(input)) != EOF) {
        ++bytes_processed;
        uint8_t k = (uint8_t)byte_in;  // Next byte to process

        // Check if sequence W+k already exists in dictionary
        int found = ht_find(mk_key(curr_code, k));
        if (found != -1) {
            // W+k exists in dictionary - extend current sequence
            curr_code = found;
        } else {
            // W+k not in dictionary
            
            // 1. Output the code for sequence W
            bw_write_code(&bw, curr_code);
            ++codes_written;
            total_bits_written += CODE_BITS;

            // 2. Check for KwKwK pattern
            // This occurs when we have a pattern like "aba" -> "abab"
            if (dict[curr_code].prefix != -1 && dict[dict[curr_code].prefix].prefix != -1) {
                if (dict[dict[curr_code].prefix].append == dict[curr_code].append) {
                    ++kwkwk_count;
                }
            }

            // 3. Add new sequence W+k to dictionary (if space available)
            if (dict_size < MAX_DICT_SIZE) {
                // Create new dictionary entry for sequence W+k
                dict[dict_size].prefix = curr_code;           // Points to sequence W
                dict[dict_size].append = k;                   // Appends byte k
                dict[dict_size].len    = dict[curr_code].len + 1;  // Length is W's length + 1
                
                // Add to hash table for fast future lookups
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

            // 4. Start new sequence with just byte k
            curr_code = (int)k;
        }
    }

    // Output the final sequence and cleanup
    bw_write_code(&bw, curr_code);  // Output code for final sequence
    ++codes_written;
    total_bits_written += CODE_BITS;
    bw_flush(&bw);                  // Ensure all bits are written to file

    // Close files and free memory
    fclose(input);
    fclose(output);
    free(dict);
    free(ht);

    // Calculate final metrics
    clock_t end_time = clock();
    double compression_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Display comprehensive compression metrics
    printf("=== LZW COMPRESSION METRICS ===\n");
    printf("Dictionary Performance:\n");
    printf("  - Final Dictionary Size: %d entries\n", dict_size);
    
    printf("\nCompression Statistics:\n");
    printf("  - Input Bytes Processed: %zu\n", bytes_processed);
    printf("  - Output Codes Written: %zu\n", codes_written);
    printf("  - Average Code Length: %.2f bits\n", (double)total_bits_written / codes_written);
    printf("  - Compression Time: %.3f seconds\n", compression_time);
    
    printf("\nAlgorithm Analysis:\n");
    printf("  - KwKwK Pattern Count: %zu\n", kwkwk_count);
    printf("  - Final Buffer State: %d bits remaining\n", bw.bit_count);
    
    printf("\nCompression complete!\n");
}
