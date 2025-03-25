#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <immintrin.h>

#define WIDTH 4096
#define HEIGHT 4096
#define NUM_THREADS 8
#define PIXELS_PER_REGISTER 32
#define ROWS_PER_THREAD (HEIGHT / NUM_THREADS)

typedef struct {
    uint8_t *image;
    int start_row;
    int end_row;
} ThreadData;

// Hardcoded AVX2 registers
const __m256i regs[8] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31},

    {32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63},

    {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95},

    {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
     112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127},

    {128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
     144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159},

    {160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
     176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191},

    {192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
     208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223},

    {224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
     240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255}
};

void *generate_part(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    uint8_t *image = data->image;
    int start_row = data->start_row;
    int end_row = data->end_row;

    for (int y = start_row; y < end_row; y++) {
        uint8_t *row = image + (y * WIDTH);

        // Store hardcoded registers directly
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
    generate_image("image_avx_static.pgm");
    return 0;
}
