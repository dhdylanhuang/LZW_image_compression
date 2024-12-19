#include <stdio.h>

int main(int argc, char *argv[]) {

    // Check if the user has provided the file file
    if (argc < 2) {
        printf("Usage: %s <file file>\n", argv[0]);
        return 1;
    }

    // Open file in binary mode
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Error: Cannot open file %s\n", argv[1]);
        return 1;
    }

    // Move to the end of the file
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error: Cannot seek to the end of file\n");
        fclose(file);
        return 1;
    }

    // Get the size of the file
    long fileSize = ftell(file);
    if (fileSize == -1) {
        perror("Error getting file size");
        fclose(file);
        return 1;
    }

    printf("File size: %ld bytes\n", fileSize);

    fclose(file);
    return 0;
}