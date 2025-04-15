#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#define BLOCK_SIZE 32  // AVX2 register size

// ----------------- File I/O -----------------
uint8_t* read_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) { 
        fprintf(stderr, "Error opening file: %s\n", filename); 
        exit(1); 
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    uint8_t *data = malloc(*size);
    if (!data) { 
        fprintf(stderr, "Memory allocation failed\n"); 
        exit(1); 
    }

    if (fread(data, 1, *size, file) != *size) {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }
    fclose(file);
    return data;
}

void write_file(const char *filename, uint8_t *data, size_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) { 
        fprintf(stderr, "Error writing file: %s\n", filename); 
        exit(1); 
    }

    if (fwrite(data, 1, size, file) != size) {
        fprintf(stderr, "Error writing file\n");
        exit(1);
    }
    fclose(file);
}

// ----------------- SIMD Block Compression -----------------
size_t simd_compress(uint8_t *input, size_t size, uint8_t *output) {
    size_t out_pos = 0;
    size_t blocks = size / BLOCK_SIZE;
    
    for (size_t i = 0; i < blocks; i++) {
        __m256i block = _mm256_loadu_si256((__m256i*)&input[i*BLOCK_SIZE]);
        
        // Check if all bytes are identical
        __m256i first = _mm256_set1_epi8(input[i*BLOCK_SIZE]);
        __m256i cmp = _mm256_cmpeq_epi8(block, first);
        int mask = _mm256_movemask_epi8(cmp);
        
        if (mask == 0xFFFFFFFF) {  // All bytes identical
            output[out_pos++] = 0x00;  // Uniform block marker
            output[out_pos++] = input[i*BLOCK_SIZE];
        } else {  // Store full block
            output[out_pos++] = 0xFF;  // Raw block marker
            memcpy(&output[out_pos], &input[i*BLOCK_SIZE], BLOCK_SIZE);
            out_pos += BLOCK_SIZE;
        }
    }
    
    // Handle remaining bytes (non-block aligned)
    size_t remaining = size % BLOCK_SIZE;
    if (remaining > 0) {
        output[out_pos++] = 0xFE;  // Partial block marker
        output[out_pos++] = remaining;
        memcpy(&output[out_pos], &input[blocks*BLOCK_SIZE], remaining);
        out_pos += remaining;
    }
    
    return out_pos;
}

// ----------------- Block Decompression -----------------
size_t simd_decompress(uint8_t *input, size_t size, uint8_t *output) {
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    while (in_pos < size) {
        uint8_t marker = input[in_pos++];
        
        switch(marker) {
            case 0x00: {  // Uniform block
                uint8_t value = input[in_pos++];
                memset(&output[out_pos], value, BLOCK_SIZE);
                out_pos += BLOCK_SIZE;
                break;
            }
            case 0xFF: {  // Full block
                memcpy(&output[out_pos], &input[in_pos], BLOCK_SIZE);
                out_pos += BLOCK_SIZE;
                in_pos += BLOCK_SIZE;
                break;
            }
            case 0xFE: {  // Partial block
                uint8_t count = input[in_pos++];
                memcpy(&output[out_pos], &input[in_pos], count);
                out_pos += count;
                in_pos += count;
                break;
            }
            default: {
                fprintf(stderr, "Invalid marker byte: 0x%02X\n", marker);
                exit(1);
            }
        }
    }
    
    return out_pos;
}

// ----------------- MAIN -----------------
int main() {
    const char* input_filename = "frank.txt";
    const char* compressed_filename = "compressed.bin";
    const char* decompressed_filename = "decompressed.txt";

    // Compression
    size_t original_size;
    uint8_t *original_data = read_file(input_filename, &original_size);
    
    uint8_t *compressed_data = malloc(original_size * 2);  // Safe allocation
    size_t compressed_size = simd_compress(original_data, original_size, compressed_data);
    
    write_file(compressed_filename, compressed_data, compressed_size);
    free(original_data);
    printf("Compression ratio: %.2f%%\n", 
           (compressed_size * 100.0) / original_size);

    // Decompression
    size_t compressed_file_size;
    uint8_t *compressed_file_data = read_file(compressed_filename, &compressed_file_size);
    
    uint8_t *decompressed_data = malloc(original_size);
    size_t decompressed_size = simd_decompress(compressed_file_data, compressed_file_size, decompressed_data);
    
    write_file(decompressed_filename, decompressed_data, decompressed_size);
    free(compressed_file_data);
    free(decompressed_data);

    printf("Decompression successful.\n");
    return 0;
}