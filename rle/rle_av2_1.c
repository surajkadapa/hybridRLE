#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#define MAX_FILE_SIZE 10 * 1024 * 1024  // 10MB max file size

// RLE Compress using AVX2
void rle_compress_avx2(uint8_t *data, size_t size, FILE *output) {
    size_t i = 0;
    while (i < size) {
        uint8_t ch = data[i];
        size_t run_length = 1;

        while (i + run_length < size && data[i + run_length] == ch && run_length < 255) {
            run_length++;
        }

        fwrite(&ch, 1, 1, output);
        fwrite(&run_length, 1, 1, output);
        i += run_length;
    }
}

// RLE Decompress
void rle_decompress(FILE *input, uint8_t *output, size_t *output_size) {
    size_t i = 0;
    while (!feof(input)) {
        uint8_t ch, run_length;
        if (fread(&ch, 1, 1, input) != 1) break;
        if (fread(&run_length, 1, 1, input) != 1) break;

        memset(&output[i], ch, run_length);
        i += run_length;
    }
    *output_size = i;
}

int main() {
    FILE *input = fopen("gatsby.txt", "rb");
    if (!input) { printf("Error opening input file\n"); return 1; }

    // Read input text
    uint8_t *data = malloc(MAX_FILE_SIZE);
    size_t size = fread(data, 1, MAX_FILE_SIZE, input);
    fclose(input);

    // Compress
    FILE *compressed = fopen("compressed.bin", "wb");
    rle_compress_avx2(data, size, compressed);
    fclose(compressed);
    printf("Compression complete!\n");

    // Decompress
    FILE *comp_input = fopen("compressed.bin", "rb");
    uint8_t *decompressed_data = malloc(MAX_FILE_SIZE);
    size_t decompressed_size = 0;

    rle_decompress(comp_input, decompressed_data, &decompressed_size);
    fclose(comp_input);

    // Write decompressed text
    FILE *output = fopen("decompressed.txt", "wb");
    fwrite(decompressed_data, 1, decompressed_size, output);
    fclose(output);

    printf("Decompression complete!\n");

    // Clean up
    free(data);
    free(decompressed_data);

    return 0;
}
