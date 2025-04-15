#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Dynamic data structures instead of fixed arrays
typedef struct {
    char** values;    // 2D array for categorical values
    int* counts;      // Count of each value
    int* yes_counts;  // Count of "Yes" for each value
    int* no_counts;   // Count of "No" for each value
    int unique_count; // Number of unique values found
} ColumnStats;

typedef struct {
    char** data;      // Dynamically allocated 2D array [row][col]
    int rows;         // Number of rows
    int cols;         // Number of columns
    char** headers;   // Column headers
} Dataset;

// Function prototypes
double log2_safe(double x);
double entropy(int yes, int no);
double total_entropy(Dataset* dataset);
double information_gain_for_column(Dataset* dataset, int col_idx);
ColumnStats* count_unique_values(Dataset* dataset, int col_idx);
void free_column_stats(ColumnStats* stats);
Dataset* read_csv(const char* filename, int max_rows);
void free_dataset(Dataset* dataset);
char* strdup_safe(const char* src);

// Main function
int main(int argc, char *argv[]) {
    const char* filename = (argc > 1) ? argv[1] : "data.csv";
    int max_rows = (argc > 2) ? atoi(argv[2]) : 100000000; // Default 100M or specified limit
    
    printf("Starting ID3 entropy calculation for up to %d rows\n", max_rows);
    
    clock_t start_total = clock();
    
    Dataset* dataset = read_csv(filename, max_rows);
    if (!dataset) {
        fprintf(stderr, "Failed to load dataset\n");
        return 1;
    }
    
    printf("Loaded %d rows, %d columns from %s\n", dataset->rows, dataset->cols, filename);
    
    clock_t start_calc = clock();
    double total_ent = total_entropy(dataset);
    printf("Total Entropy: %.4lf\n", total_ent);
    
    // Calculate information gain for each feature
    for (int i = 0; i < dataset->cols - 1; i++) {
        clock_t start = clock();
        double gain = information_gain_for_column(dataset, i);
        clock_t end = clock();
        
        printf("Info Gain (%s): %.4lf (%.3f seconds)\n", 
               dataset->headers[i], gain, (double)(end - start) / CLOCKS_PER_SEC);
    }
    
    clock_t end_total = clock();
    printf("Total time: %.3f seconds\n", (double)(end_total - start_total) / CLOCKS_PER_SEC);
    
    // Clean up
    free_dataset(dataset);
    
    return 0;
}

// Safe log2 function to handle edge cases
double log2_safe(double x) {
    return (x <= 0.0) ? 0.0 : log2(x);
}

// Calculate entropy from yes/no counts
double entropy(int yes, int no) {
    int total = yes + no;
    if (total == 0) return 0.0;
    double p_yes = (double)yes / total;
    double p_no = (double)no / total;
    return -p_yes * log2_safe(p_yes) - p_no * log2_safe(p_no);
}

// Calculate total entropy of the dataset
double total_entropy(Dataset* dataset) {
    int yes = 0, no = 0;
    int target_col = dataset->cols - 1;
    
    for (int i = 0; i < dataset->rows; i++) {
        if (strcmp(dataset->data[i][target_col], "Yes") == 0) yes++;
        else if (strcmp(dataset->data[i][target_col], "No") == 0) no++;
    }
    
    printf("Total dataset: %d yes, %d no\n", yes, no);
    return entropy(yes, no);
}

// Safe strdup implementation
char* strdup_safe(const char* src) {
    size_t len = strlen(src) + 1;
    char* dst = malloc(len);
    if (dst == NULL) return NULL;
    return memcpy(dst, src, len);
}

// Count unique values in a column and their yes/no distributions
ColumnStats* count_unique_values(Dataset* dataset, int col_idx) {
    ColumnStats* stats = malloc(sizeof(ColumnStats));
    if (!stats) return NULL;
    
    // Initial capacity (can grow if needed)
    int capacity = 100;
    stats->values = malloc(capacity * sizeof(char*));
    stats->counts = malloc(capacity * sizeof(int));
    stats->yes_counts = malloc(capacity * sizeof(int));
    stats->no_counts = malloc(capacity * sizeof(int));
    stats->unique_count = 0;
    
    if (!stats->values || !stats->counts || !stats->yes_counts || !stats->no_counts) {
        free_column_stats(stats);
        return NULL;
    }
    
    int target_col = dataset->cols - 1;
    
    // Find and count unique values
    for (int i = 0; i < dataset->rows; i++) {
        const char* value = dataset->data[i][col_idx];
        int idx = -1;
        
        // Check if we've seen this value before
        for (int j = 0; j < stats->unique_count; j++) {
            if (strcmp(value, stats->values[j]) == 0) {
                idx = j;
                break;
            }
        }
        
        if (idx >= 0) {
            // Increment existing value counts
            stats->counts[idx]++;
            if (strcmp(dataset->data[i][target_col], "Yes") == 0)
                stats->yes_counts[idx]++;
            else
                stats->no_counts[idx]++;
        } else {
            // Add new value
            if (stats->unique_count >= capacity) {
                // Grow the arrays if needed
                capacity *= 2;
                stats->values = realloc(stats->values, capacity * sizeof(char*));
                stats->counts = realloc(stats->counts, capacity * sizeof(int));
                stats->yes_counts = realloc(stats->yes_counts, capacity * sizeof(int));
                stats->no_counts = realloc(stats->no_counts, capacity * sizeof(int));
                
                if (!stats->values || !stats->counts || !stats->yes_counts || !stats->no_counts) {
                    free_column_stats(stats);
                    return NULL;
                }
            }
            
            stats->values[stats->unique_count] = strdup_safe(value);
            stats->counts[stats->unique_count] = 1;
            stats->yes_counts[stats->unique_count] = 0;
            stats->no_counts[stats->unique_count] = 0;
            
            if (strcmp(dataset->data[i][target_col], "Yes") == 0)
                stats->yes_counts[stats->unique_count] = 1;
            else
                stats->no_counts[stats->unique_count] = 1;
                
            stats->unique_count++;
        }
    }
    
    return stats;
}

// Calculate information gain for a specific column
double information_gain_for_column(Dataset* dataset, int col_idx) {
    ColumnStats* stats = count_unique_values(dataset, col_idx);
    if (!stats) return 0.0;
    
    printf("Column %s has %d unique values\n", dataset->headers[col_idx], stats->unique_count);
    
    double weighted_entropy = 0.0;
    double total_examples = dataset->rows;
    
    for (int i = 0; i < stats->unique_count; i++) {
        int yes = stats->yes_counts[i];
        int no = stats->no_counts[i];
        int count = stats->counts[i];
        
        double subset_entropy = entropy(yes, no);
        double weight = (double)count / total_examples;
        weighted_entropy += weight * subset_entropy;
        
        printf("  Value '%s': %d examples (%d yes, %d no), entropy: %.4f, weight: %.4f\n", 
               stats->values[i], count, yes, no, subset_entropy, weight);
    }
    
    double information_gain = total_entropy(dataset) - weighted_entropy;
    
    free_column_stats(stats);
    return information_gain;
}

// Free memory allocated for column statistics
void free_column_stats(ColumnStats* stats) {
    if (!stats) return;
    
    if (stats->values) {
        for (int i = 0; i < stats->unique_count; i++) {
            free(stats->values[i]);
        }
        free(stats->values);
    }
    
    free(stats->counts);
    free(stats->yes_counts);
    free(stats->no_counts);
    free(stats);
}

// Read CSV file into a dynamically allocated dataset
Dataset* read_csv(const char* filename, int max_rows) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("File open error");
        return NULL;
    }
    
    Dataset* dataset = malloc(sizeof(Dataset));
    if (!dataset) {
        fclose(f);
        return NULL;
    }
    
    // Initialize dataset
    dataset->rows = 0;
    dataset->cols = 0;
    dataset->data = NULL;
    dataset->headers = NULL;
    
    char line[4096];  // Increased buffer for wider data
    
    // Read header
    if (fgets(line, sizeof(line), f)) {
        // Remove newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = 0;
        
        // Count columns
        int col_count = 0;
        char* tmp = strdup_safe(line);
        char* token = strtok(tmp, ",");
        while (token) {
            col_count++;
            token = strtok(NULL, ",");
        }
        free(tmp);
        
        // Allocate header array
        dataset->cols = col_count;
        dataset->headers = malloc(col_count * sizeof(char*));
        if (!dataset->headers) {
            free(dataset);
            fclose(f);
            return NULL;
        }
        
        // Parse headers
        token = strtok(line, ",");
        for (int i = 0; i < col_count && token; i++) {
            dataset->headers[i] = strdup_safe(token);
            token = strtok(NULL, ",");
        }
    }
    
    // Allocate initial data buffer (can grow if needed)
    int capacity = 10000;  // Initial capacity
    dataset->data = malloc(capacity * sizeof(char**));
    if (!dataset->data) {
        for (int i = 0; i < dataset->cols; i++)
            free(dataset->headers[i]);
        free(dataset->headers);
        free(dataset);
        fclose(f);
        return NULL;
    }
    
    // Read data rows
    int milestone = max_rows / 100;  // Report progress every 1%
    if (milestone < 1) milestone = 1;
    
    time_t start_time = time(NULL);
    time_t last_report = start_time;
    
    while (fgets(line, sizeof(line), f) && dataset->rows < max_rows) {
        // Remove newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = 0;
        
        // Grow buffer if needed
        if (dataset->rows >= capacity) {
            capacity *= 2;
            dataset->data = realloc(dataset->data, capacity * sizeof(char**));
            if (!dataset->data) {
                fprintf(stderr, "Memory allocation failed at row %d\n", dataset->rows);
                fclose(f);
                return dataset;  // Return what we have so far
            }
        }
        
        // Allocate row
        dataset->data[dataset->rows] = malloc(dataset->cols * sizeof(char*));
        if (!dataset->data[dataset->rows]) {
            fprintf(stderr, "Memory allocation failed for row %d\n", dataset->rows);
            fclose(f);
            return dataset;  // Return what we have so far
        }
        
        // Parse row
        char* tmp = strdup_safe(line);
        char* token = strtok(tmp, ",");
        int col = 0;
        
        while (token && col < dataset->cols) {
            dataset->data[dataset->rows][col] = strdup_safe(token);
            token = strtok(NULL, ",");
            col++;
        }
        
        free(tmp);
        
        // Only count complete rows
        if (col == dataset->cols) {
            dataset->rows++;
        } else {
            // Free the incomplete row
            for (int i = 0; i < col; i++) {
                free(dataset->data[dataset->rows][i]);
            }
            free(dataset->data[dataset->rows]);
        }
        
        // Show progress
        if (dataset->rows % milestone == 0) {
            time_t now = time(NULL);
            if (difftime(now, last_report) >= 2.0) {  // Update at most every 2 seconds
                last_report = now;
                float percent = (float)dataset->rows / max_rows * 100.0f;
                int elapsed = (int)difftime(now, start_time);
                
                int rows_per_sec = elapsed > 0 ? dataset->rows / elapsed : 0;
                int est_remaining = rows_per_sec > 0 ? (max_rows - dataset->rows) / rows_per_sec : 0;
                
                printf("\rReading: %.1f%% complete (%d/%d rows, %d rows/sec, est. %d sec remaining)", 
                       percent, dataset->rows, max_rows, rows_per_sec, est_remaining);
                fflush(stdout);
            }
        }
    }
    
    printf("\rReading: 100.0%% complete (%d rows read)%s\n", dataset->rows, "                    ");
    
    fclose(f);
    return dataset;
}

// Free all memory allocated for the dataset
void free_dataset(Dataset* dataset) {
    if (!dataset) return;
    
    // Free headers
    if (dataset->headers) {
        for (int i = 0; i < dataset->cols; i++) {
            free(dataset->headers[i]);
        }
        free(dataset->headers);
    }
    
    // Free data
    if (dataset->data) {
        for (int i = 0; i < dataset->rows; i++) {
            if (dataset->data[i]) {
                for (int j = 0; j < dataset->cols; j++) {
                    free(dataset->data[i][j]);
                }
                free(dataset->data[i]);
            }
        }
        free(dataset->data);
    }
    
    free(dataset);
}