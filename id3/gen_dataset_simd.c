#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <immintrin.h> // For AVX-256 intrinsics

#define MAX_ROWS 10000
#define MAX_COLS 5
#define MAX_VAL_LEN 10
#define SIMD_LANES 8 // AVX-256 processes 8 int32_t at a time

const char* outlook_vals[] = {"Sunny", "Overcast", "Rain"};
const char* temp_vals[] = {"Hot", "Mild", "Cool"};
const char* humidity_vals[] = {"High", "Normal"};
const char* windy_vals[] = {"True", "False"};
const char* play_vals[] = {"Yes", "No"};

// Number of possible values per column
const int val_counts[MAX_COLS] = {3, 3, 2, 2, 2};

void generate_rows_simd(char data[SIMD_LANES][MAX_COLS][MAX_VAL_LEN], int count) {
    int indices[SIMD_LANES];

    for (int col = 0; col < MAX_COLS; col++) {
        // Generate scalar random indices and load into vector
        for (int i = 0; i < SIMD_LANES; i++) {
            indices[i] = rand() % val_counts[col];
        }
        __m256i rand_vec = _mm256_loadu_si256((__m256i*)indices);

        // Store indices back (optional, for clarity)
        _mm256_storeu_si256((__m256i*)indices, rand_vec);

        // Assign string values for each row in this column
        for (int row = 0; row < SIMD_LANES; row++) {
            const char** vals;
            switch (col) {
                case 0: vals = outlook_vals; break;
                case 1: vals = temp_vals; break;
                case 2: vals = humidity_vals; break;
                case 3: vals = windy_vals; break;
                case 4: vals = play_vals; break;
                default: vals = play_vals; // Fallback
            }
            snprintf(data[row][col], MAX_VAL_LEN, "%s", vals[indices[row]]);
        }
    }
}

void generate_csv(const char* filename, int row_count) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("File open error");
        exit(1);
    }

    // Write header
    fprintf(f, "Outlook,Temp,Humidity,Windy,Play\n");

    // Buffer for SIMD_LANES rows
    char data[SIMD_LANES][MAX_COLS][MAX_VAL_LEN];

    // Process rows in batches of SIMD_LANES
    for (int i = 0; i < row_count; i += SIMD_LANES) {
        int rows_to_generate = (i + SIMD_LANES <= row_count) ? SIMD_LANES : row_count - i;

        // Generate SIMD_LANES rows (or fewer for the last batch)
        generate_rows_simd(data, rows_to_generate);

        // Write generated rows to file
        for (int j = 0; j < rows_to_generate; j++) {
            fprintf(f, "%s,%s,%s,%s,%s\n",
                    data[j][0], data[j][1], data[j][2], data[j][3], data[j][4]);
        }
    }

    fclose(f);
}

int main() {
    srand(time(NULL)); // Seed the random number generator

    // Generate a smaller dataset for testing (10,000 rows)
    generate_csv("large_data.csv", MAX_ROWS);
    printf("Dataset generated: large_data.csv\n");

    return 0;
}