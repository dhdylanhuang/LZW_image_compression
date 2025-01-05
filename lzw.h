#ifndef LZW_H
#define LZW_H

#define MAX_DICT_SIZE 256
#define INIT_DICT_SIZE 0

void lzw_compress(const char *input_file, const char *output_file);
void lzw_decompress(const char *input_file, const char *output_file);

#endif
