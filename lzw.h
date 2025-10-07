#ifndef LZW_H
#define LZW_H

#define INIT_DICT_SIZE 256
#define CODE_BITS 14
#define MAX_DICT_SIZE 16384

#if MAX_DICT_SIZE > (1 << CODE_BITS)
#error "MAX_DICT_SIZE must be <= 2^CODE_BITS to encode within CODE_BITS bits"
#endif

void lzw_compress(const char *input_file, const char *output_file);
void lzw_decompress(const char *input_file, const char *output_file);

#endif
