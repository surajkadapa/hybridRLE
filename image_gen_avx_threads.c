#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <immintrin.h>

#define WIDTH 4096
#define HEIGHT 4096
#define NUM_THREADS 8
#define PIXELS_PER_REGISTER 64
#define ROWS_PER_THREAD (HEIGHT / NUM_THREADS)

typedef struct {
    uint8_t *image;
    int start_row;
    int end_row;
} ThreadData;

void *generate_part(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    uint8_t *image = data->image;
    int start_row = data->start_row;
    int end_row = data->end_row;

    // Precompute 8 AVX2 registers with values
    __m256i regs[8];
    for (int i = 0; i < 8; i++) {
        regs[i] = _mm256_setr_epi8(
            i * 32 + 0, i * 32 + 1, i * 32 + 2, i * 32 + 3, i * 32 + 4, i * 32 + 5, i * 32 + 6, i * 32 + 7,
            i * 32 + 8, i * 32 + 9, i * 32 + 10, i * 32 + 11, i * 32 + 12, i * 32 + 13, i * 32 + 14, i * 32 + 15,
            i * 32 + 16, i * 32 + 17, i * 32 + 18, i * 32 + 19, i * 32 + 20, i * 32 + 21, i * 32 + 22, i * 32 + 23,
            i * 32 + 24, i * 32 + 25, i * 32 + 26, i * 32 + 27, i * 32 + 28, i * 32 + 29, i * 32 + 30, i * 32 + 31
        );
    }

    for (int y = start_row; y < end_row; y++) {
        uint8_t *row = image + (y * WIDTH);

        // Store precomputed registers directly
        for (int i = 0; i < 8; i++) {
            _mm256_storeu_si256((__m256i*)&row[i * 32], regs[i]);
        }
    }

    pthread_exit(NULL);
}

void generate_image(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("[ERROR] Cannot open file\n");
        return;
    }

    fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);

    uint8_t *image = aligned_alloc(32, WIDTH * HEIGHT);
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];

    // Spawn threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].image = image;
        thread_data[i].start_row = i * ROWS_PER_THREAD;
        thread_data[i].end_row = (i + 1) * ROWS_PER_THREAD;
        pthread_create(&threads[i], NULL, generate_part, &thread_data[i]);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    fwrite(image, 1, WIDTH * HEIGHT, file);
    fclose(file);
    free(image);

    printf("[MESSAGE] Image generated successfully\n");
}

int main() {
    generate_image("image_avx_threads.pgm");
    return 0;
}
