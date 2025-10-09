#include <stdio.h>
#include <stdlib.h>
#include "lzw.h"

long get_file_size(const char *filename);
void compress_file(const char *input_file, const char *output_file);

int main(int argc, char *argv[]) {

    // Check if the user has provided the file file
    if (argc != 3) {
        printf("Usage: %s <file file>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    long original_size = get_file_size(input_file);

    if (original_size == -1) {
        fprintf(stderr, "Error: Cannot get the size of the file\n");
        return 1;
    }

    printf("Original file size: %ld bytes\n", original_size);

    // Compress the file
    compress_file(input_file, output_file);

    long compressed_size = get_file_size(output_file);
    if (compressed_size == -1) {
        fprintf(stderr, "Error: Cannot get the size of the compressed file\n");
        return 1;
    }

    printf("Compressed file size: %ld bytes\n", compressed_size);

    printf("Compression ratio: %.2f%%\n", (1 - (double)compressed_size / original_size) * 100);

    return 0;
}

long get_file_size(const char *filename) {
// Open file in binary mode
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", filename);
        return -1;
    }

    // Move to the end of the file
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error: Cannot seek to the end of file\n");
        fclose(file);
        return -1;
    }

    // Get the size of the file
    long size = ftell(file);

    if (size == -1) {
        perror("Error getting file size");
        fclose(file);
        return -1;
    }

    fclose(file);

    return size;
}

void compress_file(const char *input_file, const char *output_file) {
    // Call LZW compression
    lzw_compress(input_file, output_file);
}