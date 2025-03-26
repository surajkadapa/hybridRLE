#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>  // AVX2
#include <stdlib.h>
#include <x86intrin.h>  // For _rdrand32_step

#define WIDTH 1024
#define HEIGHT 1024

// Function to generate a 256-bit vector with random values using RDRAND
__m256i generate_random_pixels() {
    uint8_t random_values[32];

    for (int i = 0; i < 32; i += 4) {
        uint32_t rnd;
        _rdrand32_step(&rnd);  // Guaranteed to be supported
        *(uint32_t*)&random_values[i] = rnd;
    }

    return _mm256_loadu_si256((__m256i*)random_values);
}

void generate_image_avx2(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("[ERROR] Cannot open file\n");
        return;
    }

    fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);

    uint8_t *row = (uint8_t*) aligned_alloc(64, WIDTH);

    for (int y = 0; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x += 32) {
        __m256i pixels = generate_random_pixels();
        _mm256_store_si256((__m256i*)&row[x], pixels);
    }
      fwrite(row, 1, WIDTH, file);
    }

    free(row);
    fclose(file);
    printf("[MESSAGE] Image generated successfully\n");
}

int main() {
    generate_image_avx2("images/image_avx_random.pgm");
    return 0;
}
