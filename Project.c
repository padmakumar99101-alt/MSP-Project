#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 10
#define MAX_LINE 256

typedef enum { TEMP = 0, HUMIDITY = 1, PRESSURE = 2 } SensorId;

typedef struct {
    SensorId sensor;
    double value;
    time_t timestamp;
} Reading;

typedef struct {
    Reading *items;
    size_t count;
    size_t capacity;
    Reading rolling[BUFFER_SIZE];
    size_t next;
    size_t buffered;
} SensorLog;

static const char *sensor_name(SensorId id) {
    switch (id) {
        case TEMP: return "Temperature (C)";
        case HUMIDITY: return "Humidity (%)";
        case PRESSURE: return "Pressure (hPa)";
        default: return "Unknown";
    }
}

static int read_int(const char *prompt, int *out) {
    char line[MAX_LINE];
    char extra;
    printf("%s", prompt);
    if (!fgets(line, sizeof line, stdin)) return 0;
    return sscanf(line, " %d %c", out, &extra) == 1;
}

static int read_double(const char *prompt, double *out) {
    char line[MAX_LINE];
    char extra;
    printf("%s", prompt);
    if (!fgets(line, sizeof line, stdin)) return 0;
    return sscanf(line, " %lf %c", out, &extra) == 1;
}

static void format_time(time_t value, char *out, size_t size) {
    struct tm *tm_value = localtime(&value);
    if (tm_value) strftime(out, size, "%Y-%m-%d %H:%M:%S", tm_value);
    else snprintf(out, size, "unknown-time");
}

static int reserve_log(SensorLog *log) {
    if (log->count < log->capacity) return 1;
    size_t new_capacity = log->capacity ? log->capacity * 2 : 16;
    if (new_capacity < log->capacity || new_capacity > ((size_t)-1) / sizeof(Reading)) return 0;
    Reading *grown = (Reading *)realloc(log->items, new_capacity * sizeof(Reading));
    if (!grown) return 0;
    log->items = grown;
    log->capacity = new_capacity;
    return 1;
}

static int add_reading(SensorLog *log, SensorId sensor, double value, time_t timestamp) {
    if (!reserve_log(log)) return 0;
    Reading reading = { sensor, value, timestamp };
    log->items[log->count++] = reading;
    log->rolling[log->next] = reading;
    log->next = (log->next + 1) % BUFFER_SIZE;
    if (log->buffered < BUFFER_SIZE) log->buffered++;
    return 1;
}

static int is_anomaly(SensorId sensor, double value) {
    if (sensor == TEMP) return value < -10.0 || value > 50.0;
    if (sensor == HUMIDITY) return value < 0.0 || value > 100.0;
    return value < 800.0 || value > 1200.0;
}

static void generate_readings(SensorLog *log, int samples, int interval_seconds) {
    const double bases[3] = { 22.0, 48.0, 1013.0 };
    const double spans[3] = { 8.0, 28.0, 18.0 };
    time_t start = time(NULL) - (time_t)(samples > 0 ? samples - 1 : 0) * interval_seconds;
    int added = 0;
    for (int i = 0; i < samples; i++) {
        for (int s = 0; s < 3; s++) {
            double value = bases[s] + ((i * 7 + s * 11) % 17 - 8) * spans[s] / 8.0;
            if (i > 0 && i % 7 == 0) value = s == TEMP ? 58.0 : (s == HUMIDITY ? 108.0 : 1250.0);
            if (add_reading(log, (SensorId)s, value, start + (time_t)i * interval_seconds)) added++;
            else { puts("Memory allocation failed; generation stopped."); return; }
        }
    }
    printf("Generated %d timestamped readings (%d samples x 3 sensors).\n", added, samples);
}

static void add_manual_reading(SensorLog *log) {
    int choice;
    double value;
    if (!read_int("Sensor (1=temperature, 2=humidity, 3=pressure): ", &choice)) { puts("Invalid sensor choice."); return; }
    if (choice < 1 || choice > 3) { puts("Choose 1, 2, or 3."); return; }
    if (!read_double("Reading value: ", &value)) { puts("Invalid number."); return; }
    if (!add_reading(log, (SensorId)(choice - 1), value, time(NULL))) { puts("Memory allocation failed."); return; }
    printf("Saved %s = %.2f\n", sensor_name((SensorId)(choice - 1)), value);
    if (is_anomaly((SensorId)(choice - 1), value)) puts("ALERT: value is outside the configured threshold.");
}

static void show_analytics(const SensorLog *log) {
    if (log->count == 0) { puts("No readings yet. Generate or add readings first."); return; }
    puts("\nSensor analytics (min / max / average / trend):");
    for (int s = 0; s < 3; s++) {
        double min = 0, max = 0, sum = 0, first = 0, last = 0;
        size_t n = 0;
        for (size_t i = 0; i < log->count; i++) if (log->items[i].sensor == (SensorId)s) {
            double v = log->items[i].value;
            if (!n) { min = max = first = v; }
            if (v < min) min = v;
            if (v > max) max = v;
            sum += v; last = v; n++;
        }
        if (!n) { printf("%-18s no data\n", sensor_name((SensorId)s)); continue; }
        const char *trend = last > first ? "rising" : last < first ? "falling" : "steady";
        printf("%-18s min %.2f | max %.2f | avg %.2f | %s\n", sensor_name((SensorId)s), min, max, sum / n, trend);
    }
}

static void show_anomalies(const SensorLog *log) {
    size_t found = 0;
    for (size_t i = 0; i < log->count; i++) if (is_anomaly(log->items[i].sensor, log->items[i].value)) {
        char stamp[32];
        format_time(log->items[i].timestamp, stamp, sizeof stamp);
        printf("ALERT %-18s %8.2f at %s\n", sensor_name(log->items[i].sensor), log->items[i].value, stamp);
        found++;
    }
    if (!found) puts("No threshold breaches detected.");
    else printf("Total threshold breaches: %zu\n", found);
}

static void show_rolling_buffer(const SensorLog *log) {
    if (!log->buffered) { puts("Rolling buffer is empty."); return; }
    puts("\nMost recent readings (oldest to newest):");
    size_t start = (log->next + BUFFER_SIZE - log->buffered) % BUFFER_SIZE;
    for (size_t i = 0; i < log->buffered; i++) {
        const Reading *r = &log->rolling[(start + i) % BUFFER_SIZE];
        char stamp[32]; format_time(r->timestamp, stamp, sizeof stamp);
        printf("%-18s %8.2f  %s\n", sensor_name(r->sensor), r->value, stamp);
    }
}

static void export_csv(const SensorLog *log) {
    if (!log->count) { puts("No readings to export."); return; }
    char filename[MAX_LINE];
    printf("CSV filename [sensor_readings.csv]: ");
    if (!fgets(filename, sizeof filename, stdin)) return;
    filename[strcspn(filename, "\r\n")] = '\0';
    if (!filename[0]) strcpy(filename, "sensor_readings.csv");
    FILE *file = fopen(filename, "w");
    if (!file) { perror("Could not open CSV file"); return; }
    fputs("sensor,value,timestamp\n", file);
    for (size_t i = 0; i < log->count; i++) {
        char stamp[32]; format_time(log->items[i].timestamp, stamp, sizeof stamp);
        fprintf(file, "%s,%.2f,%s\n", sensor_name(log->items[i].sensor), log->items[i].value, stamp);
    }
    if (fclose(file) == 0) printf("Exported %zu rows to %s.\n", log->count, filename);
    else puts("Error closing CSV file.");
}

static void show_memory(const SensorLog *log) {
    printf("Stored readings: %zu | allocated slots: %zu | rolling buffer: %zu/%d\n", log->count, log->capacity, log->buffered, BUFFER_SIZE);
}

static void print_menu(void) {
    puts("\n=== IoT Sensor Data Logger & Analyzer ===");
    puts("1. Generate readings");
    puts("2. Add a manual reading");
    puts("3. Show min/max/average and trend per sensor");
    puts("4. Detect threshold breaches");
    puts("5. Show rolling circular buffer");
    puts("6. Export all readings to CSV");
    puts("7. Show memory and log summary");
    puts("0. Exit");
}

int main(void) {
    SensorLog log = { 0 };
    for (;;) {
        print_menu();
        int choice;
        if (!read_int("Select an option: ", &choice)) {
            puts("Input closed. Memory released.");
            break;
        }
        if (choice == 0) break;
        switch (choice) {
            case 1: {
                int samples, interval;
                if (!read_int("Number of samples (1-10000): ", &samples) || samples < 1 || samples > 10000) { puts("Enter a sample count from 1 to 10000."); break; }
                if (!read_int("Interval between samples in seconds (1-86400): ", &interval) || interval < 1 || interval > 86400) { puts("Enter an interval from 1 to 86400 seconds."); break; }
                generate_readings(&log, samples, interval);
                break;
            }
            case 2: add_manual_reading(&log); break;
            case 3: show_analytics(&log); break;
            case 4: show_anomalies(&log); break;
            case 5: show_rolling_buffer(&log); break;
            case 6: export_csv(&log); break;
            case 7: show_memory(&log); break;
            default: puts("Choose an option from 0 to 7."); break;
        }
    }
    free(log.items);
    log.items = NULL;
    puts("Logger closed.");
    return 0;
}