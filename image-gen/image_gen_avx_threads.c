#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <immintrin.h>

#define WIDTH 1024
#define HEIGHT 1024
#define NUM_THREADS 8
#define PIXELS_PER_REGISTER 32
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

    for (int y = start_row; y < end_row; y++) {
        uint8_t *row = image + (y * WIDTH);

        for (int x = 0; x < WIDTH; x += PIXELS_PER_REGISTER) {
            __m256i pixel_values = _mm256_set_epi8(
                (x+31) % 256, (x+30) % 256, (x+29) % 256, (x+28) % 256,
                (x+27) % 256, (x+26) % 256, (x+25) % 256, (x+24) % 256,
                (x+23) % 256, (x+22) % 256, (x+21) % 256, (x+20) % 256,
                (x+19) % 256, (x+18) % 256, (x+17) % 256, (x+16) % 256,
                (x+15) % 256, (x+14) % 256, (x+13) % 256, (x+12) % 256,
                (x+11) % 256, (x+10) % 256, (x+9) % 256,  (x+8) % 256,
                (x+7) % 256,  (x+6) % 256,  (x+5) % 256,  (x+4) % 256,
                (x+3) % 256,  (x+2) % 256,  (x+1) % 256,  (x+0) % 256
            );

            _mm256_storeu_si256((__m256i*)&row[x], pixel_values);
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
