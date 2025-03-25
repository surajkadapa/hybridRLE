#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>  // AVX2
#include <stdlib.h>

#define WIDTH 4096
#define HEIGHT 4096

void generate_image_avx2(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("[ERROR] Cannot open file\n");
        return;
    }

    // Writing the PGM header
    fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);

    // Allocate buffer for one row (aligned to 32 bytes for AVX2)
    uint8_t *row = (uint8_t*) aligned_alloc(32, WIDTH);

    // Generate one row using AVX2 and write it HEIGHT times
    for (int x = 0; x < WIDTH; x += 32) {
        __m256i pixels = _mm256_setr_epi8(
            x, x+1, x+2, x+3, x+4, x+5, x+6, x+7,
            x+8, x+9, x+10, x+11, x+12, x+13, x+14, x+15,
            x+16, x+17, x+18, x+19, x+20, x+21, x+22, x+23,
            x+24, x+25, x+26, x+27, x+28, x+29, x+30, x+31
        );
        _mm256_store_si256((__m256i*)&row[x], pixels);
    }

    // Write the generated row HEIGHT times
    for (int y = 0; y < HEIGHT; y++) {
        fwrite(row, 1, WIDTH, file);
    }

    free(row);
    fclose(file);
    printf("[MESSAGE] Image generated successfully\n");
}

int main() {
    generate_image_avx2("image_avx2.pgm");
    return 0;
}
