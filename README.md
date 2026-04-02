#VertiFarm Hardware System

An IoT hardware system for monitoring a vertical farm environment. Built on the ESP32 platform, it collects data from multiple sensors, logs readings to Google Sheets, pushes live data to the VertiFarm API, and captures timestamped images via ESP32-CAM modules uploaded to Google Drive.

---

##Repository Contents

| File | Description |
|------|-------------|
| `FYP_Hardware_Github_Version.ino` | Main sensor node firmware (ESP32-S3) |
| `FYP_ESP32CAM_Github_Version.ino` | Camera node firmware (ESP32-CAM / AI-Thinker) |

> A Google Apps Script (not included here) is required on the Google Drive side to receive and save camera images from the ESP32-CAM modules.

---

## 🔧 Hardware Requirements

### Sensor Node (ESP32-S3)
| Component | Purpose |
|-----------|---------|
| ESP32-S3 | Main microcontroller |
| DHT22 | Air temperature & humidity (GPIO 2) |
| BH1750 | Ambient light / lux (I2C: SDA GPIO 18, SCL GPIO 17) |
| DS18B20 | Water temperature (OneWire GPIO 5) |
| TDS Sensor | Total Dissolved Solids / nutrient concentration (GPIO 1) |
| pH Sensor | Water pH level (GPIO 4) |

### Cameras (ESP32-CAM — AI-Thinker)
| Component | Purpose |
|-----------|---------|
| ESP32-CAM (AI-Thinker) | Microcontroller with integrated camera |
| OV2640 Camera Module | Captures JPEG images (XGA resolution) |

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   VertiFarm Hardware                    │
│                                                         │
│  ┌─────────────┐          ┌──────────────────────────┐  │
│  │  ESP32-S3   │          │      ESP32-CAM(s)         │  │
│  │ Sensor Node │          │   (one per camera angle) │  │
│  └──────┬──────┘          └───────────┬──────────────┘  │
│         │                             │                 │
│  Reads sensors every 60s     Wakes every 5 min         │
│  (DHT22, BH1750, DS18B20,    Captures JPEG image       │
│   TDS, pH)                   Encodes to Base64          │
└─────────┼─────────────────────────────┼─────────────────┘
          │                             │
          ▼                             ▼
   ┌──────────────┐            ┌─────────────────┐
   │ Google Sheets │            │  Google Drive   │
   │  (Data Log)  │            │ (Image Storage) │
   └──────────────┘            └────────┬────────┘
          │                             │
          ▼                             │
   ┌──────────────┐                     │
   │ VertiFarm    │◄────────────────────┘
   │   Live API   │  (via Google Apps Script)
   └──────────────┘
```

---

##  Features

### Sensor Node (`FYP_Hardware_Github_Version.ino`)
- Reads 6 environmental parameters every **60 seconds**
- Averages **30 ADC samples** for pH and TDS readings to reduce noise
- Applies **temperature compensation** to TDS calculations
- Timestamps all readings using **NTP time sync** (Singapore GMT+8)
- Logs data to a **Google Sheets** spreadsheet (columns: Timestamp, Air Temp, Humidity, Lux, Water Temp, TDS, pH)
- Dual-publishes to the **VertiFarm Live API** via HTTPS POST with a device key
- Gracefully handles sensor failures by logging `N/A` instead of crashing

### Camera Node (`FYP_ESP32CAM_Github_Version.ino`)
- Captures a **JPEG image** on wake-up
- Uses **deep sleep** between captures (default: every 5 minutes) to save power
- Timestamps filenames using NTP sync (e.g. `CAM_1_20240101_120000.jpg`)
- Encodes image to **Base64** and sends via HTTPS POST to a Google Apps Script
- The Apps Script saves the image to a designated **Google Drive** folder
- Supports **multiple cameras** — set a unique `CAMERA_ID` per unit
- Retries connection up to **3 times** before giving up
- Sends image in **1000-byte chunks** to prevent memory overflow

---

## 🚀 Setup & Configuration

### 1. Prerequisites

Install the following libraries in the Arduino IDE:

**Sensor Node:**
- `WiFi` (built-in)
- `DHT sensor library` (Adafruit)
- `BH1750` (claws)
- `OneWire`
- `DallasTemperature`
- `ESP_Google_Sheet_Client`
- `HTTPClient` (built-in)
- `ArduinoJson`

**Camera Node:**
- `WiFi` (built-in)
- `WiFiClientSecure` (built-in)
- `esp_camera` (built-in with ESP32 board package)
- `Base64` library

---

### 2. Google Cloud Setup (Sensor Node)

1. Create a **Google Cloud Project** and enable the Google Sheets API.
2. Create a **Service Account** and download its private key (JSON).
3. Share your **Google Spreadsheet** with the service account email.
4. Note the **Spreadsheet ID** from the sheet URL.
5. Ensure the sheet has a tab named `ESP32-Datalogging` with headers in row 1:
   `Timestamp | Air Temp | Humidity | Lux | Water Temp | TDS | pH`

---

### 3. Google Apps Script Setup (Camera Node)

1. Go to [script.google.com](https://script.google.com) and create a new project.
2. Write or paste your Apps Script to receive the POST request and save the image to Google Drive.
3. Deploy as a **Web App** (execute as yourself, accessible to anyone).
4. Copy the deployment script ID into `myScript` in the camera firmware.

---

### 4. Firmware Configuration

#### Sensor Node — edit `FYP_Hardware_Github_Version.ino`:

```cpp
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
#define PROJECT_ID      "your_google_project_id"
#define CLIENT_EMAIL    "your-service-account@project.iam.gserviceaccount.com"

const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n";
const char spreadsheetId[] = "your_spreadsheet_id";

#define VERTIFARM_API_URL    "https://your-api-endpoint.com/api/live/reading"
#define VERTIFARM_DEVICE_KEY "your_device_key"
```

#### Camera Node — edit `FYP_ESP32CAM_Github_Version.ino`:

```cpp
const char *ssid     = "your_wifi_ssid";
const char *password = "your_wifi_password";

String myScript = "/macros/s/YOUR_SCRIPT_ID/exec";

#define CAMERA_ID "CAM_1"   // Change per camera (CAM_1, CAM_2, etc.)

uint64_t sleepDurationMin = 5;  // Capture interval in minutes
```

> ⚠️ **Never commit real credentials to a public repository.** Use placeholder values as shown above or a `secrets.h` file added to `.gitignore`.

---

### 5. Flashing

1. Select the correct board in Arduino IDE:
   - Sensor Node: `ESP32S3 Dev Module`
   - Camera Node: `AI Thinker ESP32-CAM`
2. Set baud rate to `115200`.
3. Flash the firmware and open the Serial Monitor to verify operation.

---

## 📊 Data Output

### Google Sheets (Sensor Node)
Each row appended to the `ESP32-Datalogging` tab:

| Timestamp | Air Temp (°C) | Humidity (%) | Lux | Water Temp (°C) | TDS (ppm) | pH |
|-----------|-------------|------------|-----|----------------|----------|----|
| 2024-01-01 12:00:00 | 27.3 | 65.2 | 4200.0 | 23.1 | 850 | 6.52 |

### Google Drive (Camera Node)
Images saved as: `CAM_1_20240101_120000.jpg`

### VertiFarm Live API (Sensor Node)
JSON payload sent via HTTPS POST every 60 seconds:
```json
{
  "timestamp": "2024-01-01 12:00:00",
  "air_temp": 27.3,
  "humidity": 65.2,
  "lux": 4200.0,
  "water_temp": 23.1,
  "tds": 850,
  "ph": 6.52
}
```
Sensor failures are sent as `null` rather than erroneous values.

---

## 🛠️ Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Sensor reads `N/A` | Wiring issue or sensor fault | Check connections and power supply |
| GSheet upload fails | Token auth error | Verify service account email, project ID, and private key |
| Camera init failed, restarting | Power brownout | Ensure stable 5V supply to ESP32-CAM |
| VertiFarm POST fails (HTTP error) | Wrong device key or API URL | Double-check `VERTIFARM_DEVICE_KEY` and `VERTIFARM_API_URL` |
| `Time Sync Error` in timestamp | NTP unreachable | Verify WiFi connection; NTP syncs on boot |
| Image upload timeout | Slow connection or large image | Reduce `FRAMESIZE` or increase `jpeg_quality` value |

---

## 📁 Suggested Repository Structure

```
vertifarm-hardware/
├── FYP_Hardware_Github_Version/
│   └── FYP_Hardware_Github_Version.ino
├── FYP_ESP32CAM_Github_Version/
│   └── FYP_ESP32CAM_Github_Version.ino
├── google-apps-script/
│   └── saveImageToDrive.gs        ← your Apps Script here
├── docs/
│   └── wiring_diagram.png
├── .gitignore
└── README.md
```

---

## 📝 License

This project was developed as a Final Year Project (FYP). Please check with the project owner before reusing or distributing.
