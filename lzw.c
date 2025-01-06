#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lzw.h"

typedef struct {
    int code;
    char *value;
} DictEntry;

void lzw_compress(const char *input_file, const char *output_file) {
    FILE *input = fopen(input_file, "rb");
    FILE *output = fopen(output_file, "wb");
    if (!input || !output) {
        fprintf(stderr, "Error opening files.\n");
        exit(1);
    }

    // Reserve space for the dictionary size at the beginning of the file
    int placeholder = 0;
    fwrite(&placeholder, sizeof(int), 1, output);

    // Initialize dictionary
    DictEntry dictionary[MAX_DICT_SIZE];
    int dict_size = INIT_DICT_SIZE;

    for (int i = 0; i < INIT_DICT_SIZE; i++) {
        dictionary[i].code = i;
        dictionary[i].value = malloc(2);
        dictionary[i].value[0] = (char)i;
        dictionary[i].value[1] = '\0';
    }

    char *sequence = malloc(2);
    sequence[0] = '\0';
    sequence[1] = '\0';

    int current;

    while ((current = fgetc(input)) != EOF) {
        // Extend the sequence
        char *new_sequence = malloc(strlen(sequence) + 2);
        strcpy(new_sequence, sequence);
        new_sequence[strlen(sequence)] = (char)current;
        new_sequence[strlen(sequence) + 1] = '\0';

        // Check if the sequence exists in the dictionary
        int found = -1;
        for (int i = 0; i < dict_size; i++) {
            if (strcmp(dictionary[i].value, new_sequence) == 0) {
                found = i;
                break;
            }
        }

        if (found != -1) {
            // Sequence exists, extend it
            free(sequence);
            sequence = new_sequence;
        } else {
            // Sequence doesn't exist, write code for existing sequence
            for (int i = 0; i < dict_size; i++) {
                if (strcmp(dictionary[i].value, sequence) == 0) {
                    fwrite(&dictionary[i].code, sizeof(int), 1, output);
                    break;
                }
            }

            // Add new sequence to dictionary
            if (dict_size < MAX_DICT_SIZE) {
                dictionary[dict_size].code = dict_size;
                dictionary[dict_size].value = new_sequence;
                dict_size++;
            } else {
                free(new_sequence);
            }

            // Reset sequence
            free(sequence);
            sequence = malloc(2);
            sequence[0] = (char)current;
            sequence[1] = '\0';
        }
    }

    // Write remaining sequence
    for (int i = 0; i < dict_size; i++) {
        if (strcmp(dictionary[i].value, sequence) == 0) {
            fwrite(&dictionary[i].code, sizeof(int), 1, output);
            break;
        }
    }

    // Go back and write the dictionary size at the start of the file
    fseek(output, 0, SEEK_SET);
    fwrite(&dict_size, sizeof(int), 1, output);

    // Cleanup
    free(sequence);
    for (int i = 0; i < dict_size; i++) {
        free(dictionary[i].value);
    }

    fclose(input);
    fclose(output);
    printf("Compression complete.\n");
}

