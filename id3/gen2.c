#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_ROWS 100000000 // 1M rows
#define MAX_COLS 5
#define MAX_VAL_LEN 10

const char* outlook_vals[] = {"Sunny", "Overcast", "Rain"};
const char* temp_vals[] = {"Hot", "Mild", "Cool"};
const char* humidity_vals[] = {"High", "Normal"};
const char* windy_vals[] = {"True", "False"};
const char* play_vals[] = {"Yes", "No"};

void generate_row(char data[MAX_COLS][MAX_VAL_LEN]) {
    int outlook_idx = rand() % 3;
    int temp_idx = rand() % 3;
    int humidity_idx = rand() % 2;
    int windy_idx = rand() % 2;
    int play_idx;

    // Strong correlations for Play
    if (outlook_idx == 1) { // Overcast
        play_idx = (rand() % 100 < 90) ? 0 : 1; // 90% Yes
    } else if (outlook_idx == 0 && humidity_idx == 0) { // Sunny + High
        play_idx = (rand() % 100 < 80) ? 1 : 0; // 80% No
    } else if (outlook_idx == 2 && windy_idx == 0) { // Rain + True
        play_idx = (rand() % 100 < 70) ? 1 : 0; // 70% No
    } else {
        play_idx = rand() % 2; // 50/50
    }

    snprintf(data[0], MAX_VAL_LEN, "%s", outlook_vals[outlook_idx]);
    snprintf(data[1], MAX_VAL_LEN, "%s", temp_vals[temp_idx]);
    snprintf(data[2], MAX_VAL_LEN, "%s", humidity_vals[humidity_idx]);
    snprintf(data[3], MAX_VAL_LEN, "%s", windy_vals[windy_idx]);
    snprintf(data[4], MAX_VAL_LEN, "%s", play_vals[play_idx]);
}

void generate_csv(const char* filename, int row_count) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("File open error");
        exit(1);
    }

    fprintf(f, "Outlook,Temp,Humidity,Windy,Play\n");

    char row[MAX_COLS][MAX_VAL_LEN];
    for (int i = 0; i < row_count; i++) {
        generate_row(row);
        fprintf(f, "%s,%s,%s,%s,%s\n", row[0], row[1], row[2], row[3], row[4]);
    }

    fclose(f);
}

int main() {
    srand(time(NULL));
    generate_csv("large_data.csv", MAX_ROWS);
    printf("Dataset generated: large_data.csv\n");
    return 0;
}