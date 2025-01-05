#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lzw.h"

typedef struct {
    int code;
    char *value;
} DictEntry;

// Function to initialize the dictionary for decompression based on dict_size
void initialize_dictionary(DictEntry *dictionary, int dict_size) {
    for (int i = 0; i < dict_size; i++) {
        dictionary[i].code = i;
        dictionary[i].value = malloc(2);
        dictionary[i].value[0] = (char)i;
        dictionary[i].value[1] = '\0';
    }
}

// Decompression function
void lzw_decompress(const char *input_file, const char *output_file) {
    FILE *input = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        exit(1);
    }

    // Read the dictionary size from the compressed file
    int dict_size;
    if (fread(&dict_size, sizeof(int), 1, input) != 1) {
        fprintf(stderr, "Error reading dictionary size.\n");
        exit(1);
    }

    // Initialize the dictionary based on the read dictionary size
    DictEntry dictionary[MAX_DICT_SIZE];
    initialize_dictionary(dictionary, dict_size);

    int prev_code, curr_code;
    char *sequence, *new_entry;

    // Read the first code and output its value
    if (fread(&prev_code, sizeof(int), 1, input) != 1) {
        fprintf(stderr, "Error reading the first code.\n");
        exit(1);
    }
    fwrite(dictionary[prev_code].value, 1, strlen(dictionary[prev_code].value), output);

    while (fread(&curr_code, sizeof(int), 1, input) == 1) {
        if (curr_code < dict_size) {
            // Sequence exists in the dictionary
            sequence = dictionary[curr_code].value;
        } else if (curr_code == dict_size) {
            // Special case: curr_code is not in the dictionary yet
            if (prev_code < dict_size) {
                sequence = malloc(strlen(dictionary[prev_code].value) + 2);
                strcpy(sequence, dictionary[prev_code].value);
                sequence[strlen(dictionary[prev_code].value)] = dictionary[prev_code].value[0];
                sequence[strlen(dictionary[prev_code].value) + 1] = '\0';
            } else {
                fprintf(stderr, "Error: Invalid code due to prev_code out of bounds.\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "Error: Invalid code encountered. curr_code: %d, dict_size: %d\n", curr_code, dict_size);
            exit(1);
        }

        // Output the sequence
        fwrite(sequence, 1, strlen(sequence), output);

        // Add new sequence to the dictionary
        if (dict_size < MAX_DICT_SIZE) {
            new_entry = malloc(strlen(dictionary[prev_code].value) + 2);
            strcpy(new_entry, dictionary[prev_code].value);
            new_entry[strlen(dictionary[prev_code].value)] = sequence[0];
            new_entry[strlen(dictionary[prev_code].value) + 1] = '\0';

            dictionary[dict_size].code = dict_size;
            dictionary[dict_size].value = new_entry;
            dict_size++;
        }

        prev_code = curr_code;
    }

    // Cleanup
    for (int i = 0; i < dict_size; i++) {
        free(dictionary[i].value);
    }
    fclose(input);
    fclose(output);

    printf("Decompression complete.\n");
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_compressed_file> <output_decompressed_file>\n", argv[0]);
        return 1;
    }

    lzw_decompress(argv[1], argv[2]);

    return 0;
}
