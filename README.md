# IoT Sensor Data Logger & Analyzer

A menu-driven C program for generating or recording temperature, humidity, and
pressure readings. It keeps readings in memory for the current run and provides
basic analytics, threshold alerts, a rolling view of recent readings, and CSV
export.

## Features

- Generate timestamped readings for all three sensors.
- Add individual readings manually.
- Calculate each sensor's minimum, maximum, average, and trend.
- Detect readings outside the configured thresholds:
  - Temperature: below -10 °C or above 50 °C
  - Humidity: below 0% or above 100%
  - Pressure: below 800 hPa or above 1200 hPa
- View the 10 most recent readings in oldest-to-newest order.
- Export all readings from the current run to a CSV file.
- Display the number of stored readings, allocated slots, and rolling-buffer use.

Generated readings use synthetic values and may include threshold breaches.
They are not measurements from physical sensors.

## Requirements

- A C compiler (for example, GCC/MinGW).
- No third-party libraries are required.

## Build

From the project directory, compile with GCC:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic Project.c -o Project.exe
```

In Visual Studio Code, the default C/C++ build task is configured to compile
the active file with `C:\MinGW\bin\gcc.exe`.

## Run

In PowerShell:

```powershell
.\Project.exe
```

Choose an option from the menu:

| Option | Action |
| --- | --- |
| 1 | Generate 1–10,000 samples at an interval of 1–86,400 seconds. Each sample adds one reading for each sensor. |
| 2 | Add one manual temperature, humidity, or pressure reading. |
| 3 | Show min/max/average and the trend from the first to the latest reading for each sensor. |
| 4 | List all readings outside their sensor's threshold. |
| 5 | Show the most recent 10 readings (or fewer if there are fewer than 10). |
| 6 | Export all readings to CSV. Press Enter at the filename prompt to use `sensor_readings.csv`. |
| 7 | Show the in-memory log and rolling-buffer summary. |
| 0 | Exit. |

The CSV contains the columns `sensor`, `value`, and `timestamp`. Timestamps are
formatted in the computer's local time zone. Readings are held only in memory
and are cleared when the program exits; export them before exiting if you need
to keep a record.
