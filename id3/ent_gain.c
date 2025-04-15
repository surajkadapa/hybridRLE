#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_ROWS 10000000 // 1M rows
#define MAX_COLS 5       // Outlook, Temp, Humidity, Windy, Play
#define MAX_LEN 32
#define MAX_UNIQUE 10    // Max unique values (small for your dataset)

static char headers[MAX_COLS][MAX_LEN];
static char data[MAX_ROWS][MAX_COLS][MAX_LEN];
static int row_count = 0, col_count = 0;

double log2_safe(double x) {
    return (x <= 0.0) ? 0.0 : log2(x);
}

double entropy(int yes, int no) {
    int total = yes + no;
    if (total == 0) return 0.0;
    double p_yes = (double)yes / total;
    double p_no = (double)no / total;
    return -p_yes * log2_safe(p_yes) - p_no * log2_safe(p_no);
}

double total_entropy() {
    int yes = 0, no = 0;
    for (int i = 0; i < row_count; i++) {
        if (strcmp(data[i][col_count - 1], "Yes") == 0) yes++;
        else if (strcmp(data[i][col_count - 1], "No") == 0) no++;
    }
    printf("Total dataset: %d yes, %d no\n", yes, no);
    return entropy(yes, no);
}

double information_gain_for_column(int col_idx) {
    static char unique_vals[MAX_UNIQUE][MAX_LEN]; // Static, small size
    int unique_count = 0;

    // Find unique values
    for (int i = 0; i < row_count; i++) {
        int found = 0;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp(data[i][col_idx], unique_vals[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && unique_count < MAX_UNIQUE) {
            strcpy(unique_vals[unique_count++], data[i][col_idx]);
        }
    }

    printf("Column %s has %d unique values\n", headers[col_idx], unique_count);

    double weighted_entropy = 0.0;
    double total_examples = row_count;

    // Count yes/no for each unique value
    for (int i = 0; i < unique_count; i++) {
        int yes = 0, no = 0, count = 0;
        for (int j = 0; j < row_count; j++) {
            if (strcmp(data[j][col_idx], unique_vals[i]) == 0) {
                count++;
                if (strcmp(data[j][col_count - 1], "Yes") == 0) yes++;
                else if (strcmp(data[j][col_count - 1], "No") == 0) no++;
            }
        }

        double subset_entropy = entropy(yes, no);
        double weight = (double)count / total_examples;
        weighted_entropy += weight * subset_entropy;

        printf("  Value '%s': %d examples (%d yes, %d no), entropy: %.4f, weight: %.4f\n",
               unique_vals[i], count, yes, no, subset_entropy, weight);
    }

    double information_gain = total_entropy() - weighted_entropy;
    return information_gain;
}

void read_csv(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("File open error");
        exit(1);
    }

    char line[512];
    if (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;

        char* token = strtok(line, ",");
        while (token && col_count < MAX_COLS) {
            strcpy(headers[col_count++], token);
            token = strtok(NULL, ",");
        }
    }

    while (fgets(line, sizeof(line), f) && row_count < MAX_ROWS) {
        size_t len = strlen(line);
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;

        int c = 0;
        char* token = strtok(line, ",");
        while (token && c < col_count) {
            strcpy(data[row_count][c++], token);
            token = strtok(NULL, ",");
        }
        if (c == col_count) {
            row_count++;
        }
    }

    printf("Read %d rows, %d columns from %s\n", row_count, col_count, filename);
    fclose(f);
}

int main(int argc, char *argv[]) {
    const char* filename = (argc > 1) ? argv[1] : "data.csv";
    read_csv(filename);

    printf("Total Entropy: %.4lf\n", total_entropy());
    for (int i = 0; i < col_count - 1; i++) {
        printf("Info Gain (%s): %.4lf\n", headers[i], information_gain_for_column(i));
    }
    return 0;
}