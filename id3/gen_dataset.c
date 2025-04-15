#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_ROWS 1000000  // 10 million rows
#define MAX_COLS 10       // More columns for complexity
#define MAX_VAL_LEN 16

// Feature values - increased number of values for more complexity
const char* outlook_vals[] = {"Sunny", "Overcast", "Rain", "Fog", "Snow", "Sleet", "Hail"};
const int outlook_count = 7;

const char* temp_vals[] = {"Hot", "Mild", "Cool", "Cold", "Freezing", "Warm"};
const int temp_count = 6;

const char* humidity_vals[] = {"High", "Normal", "Low", "VeryHigh", "VeryLow"};
const int humidity_count = 5;

const char* windy_vals[] = {"None", "Light", "Medium", "Strong", "Gale"};
const int windy_count = 5;

// Additional features to increase computational demands
const char* time_vals[] = {"Morning", "Afternoon", "Evening", "Night", "Dawn", "Dusk"};
const int time_count = 6;

const char* season_vals[] = {"Spring", "Summer", "Fall", "Winter"};
const int season_count = 4;

const char* forecast_vals[] = {"Improving", "Stable", "Worsening", "Unpredictable"};
const int forecast_count = 4;

const char* pressure_vals[] = {"Rising", "Stable", "Falling"};
const int pressure_count = 3;

const char* visibility_vals[] = {"Excellent", "Good", "Fair", "Poor", "VeryPoor"};
const int visibility_count = 5;

const char* play_vals[] = {"Yes", "No"};

// Generate a row with complex feature interactions
void generate_row(char data[MAX_COLS][MAX_VAL_LEN]) {
    // Randomly assign values to each feature column
    int outlook_idx = rand() % outlook_count;
    int temp_idx = rand() % temp_count;
    int humidity_idx = rand() % humidity_count;
    int windy_idx = rand() % windy_count;
    int time_idx = rand() % time_count;
    int season_idx = rand() % season_count;
    int forecast_idx = rand() % forecast_count;
    int pressure_idx = rand() % pressure_count;
    int visibility_idx = rand() % visibility_count;
    
    // Copy feature values
    snprintf(data[0], MAX_VAL_LEN, "%s", outlook_vals[outlook_idx]);
    snprintf(data[1], MAX_VAL_LEN, "%s", temp_vals[temp_idx]);
    snprintf(data[2], MAX_VAL_LEN, "%s", humidity_vals[humidity_idx]);
    snprintf(data[3], MAX_VAL_LEN, "%s", windy_vals[windy_idx]);
    snprintf(data[4], MAX_VAL_LEN, "%s", time_vals[time_idx]);
    snprintf(data[5], MAX_VAL_LEN, "%s", season_vals[season_idx]);
    snprintf(data[6], MAX_VAL_LEN, "%s", forecast_vals[forecast_idx]);
    snprintf(data[7], MAX_VAL_LEN, "%s", pressure_vals[pressure_idx]);
    snprintf(data[8], MAX_VAL_LEN, "%s", visibility_vals[visibility_idx]);
    
    // Generate target value based on complex feature interactions
    int play_yes_probability = 50; // Base 50% chance
    
    // Primary features with stronger influence
    if (outlook_idx == 1) play_yes_probability += 30; // "Overcast" strongly favors Yes
    if (outlook_idx == 0) play_yes_probability -= 15; // "Sunny" moderately against
    if (outlook_idx == 2) play_yes_probability -= 10; // "Rain" slightly against
    if (outlook_idx >= 3) play_yes_probability -= 20; // Other weather conditions more against
    
    if (temp_idx == 1 || temp_idx == 5) play_yes_probability += 15; // "Mild"/"Warm" favors
    if (temp_idx == 0) play_yes_probability -= 10; // "Hot" against
    if (temp_idx >= 3) play_yes_probability -= 20; // Cold weather against
    
    if (humidity_idx == 1) play_yes_probability += 10; // "Normal" humidity favors
    if (humidity_idx == 0 || humidity_idx == 3) play_yes_probability -= 15; // High humidity against
    
    if (windy_idx >= 3) play_yes_probability -= 25; // Strong wind against
    
    // Secondary features with moderate influence
    if (time_idx == 1 || time_idx == 2) play_yes_probability += 10; // Afternoon/Evening favors
    if (time_idx == 3) play_yes_probability -= 15; // Night against
    
    if (season_idx == 1) play_yes_probability += 15; // Summer favors
    if (season_idx == 3) play_yes_probability -= 15; // Winter against
    
    if (forecast_idx == 0) play_yes_probability += 10; // Improving forecast favors
    if (forecast_idx == 2) play_yes_probability -= 10; // Worsening forecast against
    
    if (visibility_idx >= 3) play_yes_probability -= 20; // Poor visibility against
    
    // Feature interactions (more complex correlations)
    // Rain + Windy is very bad
    if (outlook_idx == 2 && windy_idx >= 2) play_yes_probability -= 15;
    
    // Hot + High humidity is very bad
    if (temp_idx == 0 && (humidity_idx == 0 || humidity_idx == 3)) play_yes_probability -= 20;
    
    // Summer + Morning/Afternoon + Good visibility is very good
    if (season_idx == 1 && (time_idx == 0 || time_idx == 1) && visibility_idx <= 1) 
        play_yes_probability += 25;
    
    // Winter + Cold/Freezing is very bad
    if (season_idx == 3 && (temp_idx == 3 || temp_idx == 4)) play_yes_probability -= 30;
    
    // Clamp probability between 5% and 95%
    if (play_yes_probability < 5) play_yes_probability = 5;
    if (play_yes_probability > 95) play_yes_probability = 95;
    
    // Determine play value based on calculated probability
    int play_idx = (rand() % 100 < play_yes_probability) ? 0 : 1; // Yes : No
    snprintf(data[9], MAX_VAL_LEN, "%s", play_vals[play_idx]);
}

void generate_csv(const char* filename, int row_count) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("File open error");
        exit(1);
    }
    
    // Write header
    fprintf(f, "Outlook,Temp,Humidity,Windy,Time,Season,Forecast,Pressure,Visibility,Play\n");
    
    // Write data
    char row[MAX_COLS][MAX_VAL_LEN];
    
    // Display progress
    int milestone = row_count / 100; // Show progress at 1% intervals
    if (milestone < 1) milestone = 1;
    
    time_t start_time = time(NULL);
    
    for (int i = 0; i < row_count; i++) {
        generate_row(row);
        fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", 
                row[0], row[1], row[2], row[3], row[4], 
                row[5], row[6], row[7], row[8], row[9]);
        
        // Show progress
        if (i % milestone == 0) {
            float percent = (float)i / row_count * 100;
            time_t current = time(NULL);
            int elapsed = (int)difftime(current, start_time);
            
            printf("\rGenerating: %.1f%% complete (%d/%d rows, %d seconds elapsed)", 
                   percent, i, row_count, elapsed);
            fflush(stdout);
        }
    }
    
    printf("\rGenerating: 100.0%% complete (%d/%d rows)%s\n", 
           row_count, row_count, "                    ");
    
    fclose(f);
}

int main(int argc, char* argv[]) {
    srand(time(NULL)); // Seed the random number generator
    
    int rows = 1000000; // Default to 1 million rows
    
    // Allow command-line specification of row count
    if (argc > 1) {
        rows = atoi(argv[1]);
        if (rows <= 0 || rows > MAX_ROWS) {
            printf("Invalid row count. Using default of 1 million rows.\n");
            rows = 1000000;
        }
    }
    
    char filename[64];
    snprintf(filename, sizeof(filename), "id3_data_%dM.csv", rows/1000000);
    
    printf("Generating %d rows of ID3 test data to %s\n", rows, filename);
    
    time_t start_time = time(NULL);
    generate_csv(filename, rows);
    time_t end_time = time(NULL);
    
    int elapsed = (int)difftime(end_time, start_time);
    printf("Dataset generated in %d seconds\n", elapsed);
    
    return 0;
}