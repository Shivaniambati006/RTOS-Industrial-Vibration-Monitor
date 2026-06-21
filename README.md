# RTOS-Based Industrial Vibration Monitoring & Predictive Maintenance System

<p align="center">
  <img src="./images/pic.png" alt="Project Architecture and CI/CD Poster" width="80%">
</p>
<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32%20%7C%20ESP32--C3-blue?style=for-the-badge&logo=espressif" />
  <img src="https://img.shields.io/badge/RTOS-FreeRTOS-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/AI-TinyML%20%7C%20Edge%20Impulse-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Language-C%2FC%2B%2B-lightgrey?style=for-the-badge&logo=c%2B%2B" />
  <img src="https://img.shields.io/badge/Inference%20Latency-%3C%2076ms-brightgreen?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Model%20Accuracy-89%25-success?style=for-the-badge" />
</p>

## Project Overview

Industrial machinery failures account for billions of dollars in unplanned downtime annually. Traditional threshold-based monitoring systems are reactive by design — they alert only *after* a fault has occurred. This project addresses that fundamental limitation by delivering a **fully embedded, real-time predictive maintenance platform** that detects early signs of mechanical degradation *before* catastrophic failure.

Built on the **ESP32 microcontroller** and powered by **FreeRTOS**, this system continuously acquires raw vibration signals from industrial equipment, runs on-device **TinyML inference** using a trained Edge Impulse model, and streams machine health intelligence to a **live ESP32-C3 dashboard** — all with no cloud dependency and sub-100ms response latency.

This is not a prototype. Every architectural decision — from task isolation to queue-based inter-process communication — reflects **production-grade embedded systems engineering principles** suited for deployment in real industrial environments.

### Key Features

- **Concurrent Real-Time Processing** — Four independent FreeRTOS tasks (Sensor, Inference, Alert, Dashboard) run in parallel with deterministic scheduling, ensuring zero data loss and bounded response times.
- **On-Device TinyML Inference** — Vibration pattern classification runs entirely at the edge using a quantized neural network deployed via the Edge Impulse C++ SDK. No cloud round-trip required.
- **89% Classification Accuracy** — The deployed model distinguishes between normal operation, imbalance, bearing wear, and misalignment fault classes with high fidelity.
- **Sub-76ms Inference Latency** — Full DSP feature extraction plus neural network inference completes in under 76 milliseconds, enabling near-real-time fault response.
- **Machine Health Scoring** — A composite health index is computed continuously and surfaced on the live dashboard to give operators an intuitive, at-a-glance equipment status.
- **Anomaly Alerting** — Threshold-based alert logic running on a dedicated FreeRTOS task triggers immediate notifications upon fault detection, decoupled from the inference pipeline.
- **Live Trend Visualization** — The ESP32-C3 dashboard renders real-time vibration amplitude and health score trends, enabling temporal fault pattern analysis.
- **Interrupt-Driven Sensor Acquisition** — Accelerometer sampling is driven by hardware interrupts to guarantee consistent, jitter-free data capture at the configured sampling rate.
- **Low-Power & Embedded-First** — Designed specifically for resource-constrained microcontrollers; the entire system runs within the ESP32's 520 KB SRAM footprint.


## System Architecture

The system is decomposed into four independent **FreeRTOS tasks** that communicate exclusively through **queues and semaphores**, enforcing strict separation of concerns and enabling true concurrent execution across the ESP32's dual-core processor.

### Conceptual Task Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ESP32 (Dual-Core MCU)                        │
│                                                                     │
│  ┌──────────────────┐     xQueue      ┌────────────────────────┐   │
│  │  TASK 1          │ ─────────────►  │  TASK 2                │   │
│  │  Sensor          │  [Raw Vibration  │  TinyML Inference      │   │
│  │  Acquisition     │   Data Frames]   │  (Edge Impulse SDK)    │   │
│  │                  │                 │                        │   │
│  │ • MPU6050/ADXL   │                 │ • DSP Feature Extract  │   │
│  │ • Interrupt-     │                 │ • Spectral Analysis    │   │
│  │   Driven Sampler │                 │ • NN Classification    │   │
│  │ • Fixed-rate DMA │                 │ • Health Score Calc    │   │
│  └──────────────────┘                 └───────────┬────────────┘   │
│                                                   │                │
│                                    xQueue (Inference Results)      │
│                                        ┌──────────┴──────────┐     │
│                                        │                      │     │
│                              ┌─────────▼──────────┐  ┌───────▼───┐ │
│                              │  TASK 3             │  │  TASK 4   │ │
│                              │  Alert Handling     │  │  Dashboard│ │
│                              │                     │  │  Comms    │ │
│                              │ • Fault Thresholds  │  │           │ │
│                              │ • GPIO / Buzzer     │  │ • MQTT /  │ │
│                              │ • LED Indicators    │  │   WS      │ │
│                              │ • Alert Debounce    │  │ • JSON    │ │
│                              │ • Event Logging     │  │   Payload │ │
│                              └─────────────────────┘  └───────────┘ │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                                       │
                               Wi-Fi / UART
                                       │
                    ┌──────────────────▼──────────────────┐
                    │         ESP32-C3 Dashboard           │
                    │  • Machine Health Score Display      │
                    │  • Real-Time Vibration Trend Plot    │
                    │  • Live Anomaly Alert Feed           │
                    └──────────────────────────────────────┘
```

### Task Responsibilities

**Task 1 — Sensor Acquisition**
Runs on Core 0 at the highest priority. Interfaces with the accelerometer over I2C/SPI using interrupt-driven sampling at a fixed rate (e.g., 1 kHz). Raw tri-axial acceleration samples are buffered into a fixed-size frame and pushed onto `sensorDataQueue` upon completion. DMA-backed transfers prevent CPU stalls during data capture.

**Task 2 — TinyML Inference**
Runs on Core 1, blocked on `sensorDataQueue`. Upon receiving a complete sensor frame, it invokes the Edge Impulse C++ inference pipeline: DSP preprocessing (FFT-based spectral feature extraction) followed by neural network forward pass. The resulting classification label and confidence scores are packaged into an `InferenceResult_t` struct and dispatched to `inferenceResultQueue`. This task owns all machine health score computation logic.

**Task 3 — Alert Handling**
Blocked on `inferenceResultQueue`. Applies configurable fault-class thresholds and hysteresis logic to suppress transient false positives. On confirmed fault detection, it drives GPIO outputs for physical indicators (LEDs, buzzer) and writes structured alert records to a ring buffer for downstream logging. Alert state transitions are governed by a finite state machine to prevent chattering.

**Task 4 — Dashboard Communication**
Subscribes to `inferenceResultQueue` (shared with Task 3 via a broadcast pattern). Serializes health scores, classification results, raw vibration envelopes, and alert states into a JSON payload and transmits over MQTT or WebSocket to the ESP32-C3 dashboard node. Implements connection watchdog and automatic reconnect logic for robust Wi-Fi operation.


## TinyML Model Details

The predictive fault detection model was developed end-to-end using the **Edge Impulse** platform and deployed as a quantized C++ library directly onto the ESP32.

### Training Pipeline

```
Raw Accelerometer Data (X, Y, Z axes @ 1 kHz)
        │
        ▼
┌─────────────────────┐
│   Data Collection   │  ← Vibration data captured under 4 operating
│   & Labeling        │    conditions: Normal, Imbalance, Bearing Wear,
└────────┬────────────┘    Misalignment
         │
         ▼
┌─────────────────────┐
│   Preprocessing /   │  ← Sliding window segmentation (e.g., 2-second
│   Feature Extract   │    windows, 50% overlap) → FFT Spectral Analysis
│   (Spectral Block)  │    → Frequency-domain feature vectors
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│   Neural Network    │  ← Fully-connected classifier trained on spectral
│   Classifier        │    features; quantized to INT8 for MCU deployment
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│  Edge Impulse SDK   │  ← Model exported as optimized C++ library,
│  (C++ Library)      │    integrated directly into FreeRTOS firmware
└─────────────────────┘
```

### Feature Extraction — Spectral Analysis Block

The DSP stage applies a **Fast Fourier Transform (FFT)** to each windowed segment of raw accelerometer data, extracting the following features per axis:

- Spectral power across configurable frequency bins
- RMS amplitude and peak-to-peak values
- Spectral entropy and skewness
- Dominant frequency and harmonic ratios

These frequency-domain features serve as robust, rotation-speed-invariant representations of mechanical vibration signatures — making the classifier generalizable across varying equipment operating speeds.

### Classification Labels

| Class ID | Label           | Description                                  |
|----------|-----------------|----------------------------------------------|
| 0        | `NORMAL`        | Healthy baseline operation                   |
| 1        | `IMBALANCE`     | Rotational mass imbalance detected           |
| 2        | `BEARING_WEAR`  | Early-stage bearing degradation signature    |
| 3        | `MISALIGNMENT`  | Shaft or coupling misalignment detected      |

### Performance Metrics

| Metric                    | Value         |
|---------------------------|---------------|
| **Classification Accuracy** | **89%**     |
| **On-Device Inference Latency** | **< 76 ms** |
| **Model Size (INT8 quantized)** | ~45 KB   |
| **Peak RAM Usage (Inference)**  | ~28 KB   |
| **Sampling Rate**           | 1 kHz         |
| **Window Size**             | 2 seconds     |
| **Window Overlap**          | 50%           |

> The model was validated on a held-out test set representative of real-world industrial operating conditions. Quantization from FP32 to INT8 resulted in less than 1.2% accuracy degradation while achieving a 4× reduction in inference time and memory footprint.

## Dashboard & Monitoring

The **ESP32-C3** serves as a dedicated dashboard node, receiving structured telemetry from the main ESP32 inference engine over Wi-Fi (MQTT or WebSocket) and rendering a live web-based monitoring interface accessible from any browser on the local network.

### Dashboard Features

**Machine Health Score**
A composite health index (0–100) is computed from the real-time classification confidence distribution. A score of 100 indicates nominal operation; scores below configurable thresholds trigger progressive warning and critical alert states. The score is updated on every inference cycle and displayed as a large, color-coded gauge (green → yellow → red).

**Real-Time Anomaly Alerts**
A timestamped alert feed displays the most recent fault events with classification label, confidence percentage, and severity level. Alerts persist in a scrollable log for post-incident review. Critical faults trigger a visual banner notification overlaid on the dashboard.

**Vibration Trend Visualization**
A rolling time-series chart plots raw vibration amplitude (RMS envelope) across all three accelerometer axes in real time. The chart retains a configurable look-back window (e.g., 60 seconds), enabling operators to observe the evolution of vibration signatures before and after a detected anomaly.

**System Status Panel**
Displays live firmware telemetry: inference task cycle time, queue occupancy, Wi-Fi RSSI, uptime, and free heap memory — providing full system observability from the dashboard without requiring a debugger.

### Dashboard Communication Protocol

```
ESP32 (Inference Node)          ESP32-C3 (Dashboard Node)
        │                                │
        │  MQTT Topic: /machine/health   │
        │ ──────────────────────────────►│
        │                                │
        │  Payload (JSON):               │
        │  {                             │
        │    "health_score": 74,         │
        │    "fault_class": "BEARING",   │
        │    "confidence": 0.91,         │
        │    "rms_x": 0.42,              │
        │    "rms_y": 0.38,              │
        │    "rms_z": 1.21,              │
        │    "timestamp_ms": 1234567890  │
        │  }                             │
        │ ──────────────────────────────►│
        │                                │
```

## Tech Stack & Hardware

### Hardware

| Component                      | Role                                              |
|-------------------------------|---------------------------------------------------|
| **ESP32 (Xtensa LX6, Dual-Core)** | Main inference node — sensor acquisition, FreeRTOS task scheduling, TinyML inference |
| **ESP32-C3 (RISC-V)**         | Dashboard node — telemetry aggregation, live web UI serving |
| **MPU6050 / ADXL345**         | 3-axis MEMS accelerometer — vibration signal source (I2C/SPI) |
| **Status LEDs (RGB)**         | Physical fault indicator outputs driven by Alert Task |
| **Buzzer / Relay Module**     | Audible/physical alarm output for critical fault states |

### Software & Middleware

| Technology                  | Purpose                                                 |
|----------------------------|---------------------------------------------------------|
| **FreeRTOS**               | Real-time OS — task scheduling, queues, semaphores, timers |
| **Edge Impulse C++ SDK**   | On-device DSP pipeline and neural network inference engine |
| **ESP-IDF (v5.x)**         | Espressif's official IoT development framework          |
| **C / C++17**              | Firmware implementation language                        |
| **MQTT (Mosquitto)**       | Lightweight pub/sub telemetry protocol for dashboard comms |
| **WebSocket**              | Alternative real-time transport for browser-based dashboard |
| **ArduinoJSON / cJSON**    | Efficient JSON serialization for telemetry payloads     |
| **CMake / Ninja**          | Build system                                            |


## Getting Started

### Prerequisites

Ensure the following tools are installed and configured on your development machine:

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) — Espressif IoT Development Framework
- [Edge Impulse CLI](https://docs.edgeimpulse.com/docs/tools/edge-impulse-cli) — for model export and deployment
- Python 3.8+ (required by ESP-IDF and Edge Impulse toolchain)
- CMake 3.16+ and Ninja build system
- A serial terminal (e.g., `idf.py monitor`, minicom, or PuTTY)

```bash
# Verify ESP-IDF installation
idf.py --version

# Verify Edge Impulse CLI
edge-impulse-daemon --version
```

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/<your-username>/rtos-vibration-predictive-maintenance.git
cd rtos-vibration-predictive-maintenance

# 2. Initialize and update submodules (ESP-IDF components, Edge Impulse SDK)
git submodule update --init --recursive

# 3. Set up the ESP-IDF environment
source $IDF_PATH/export.sh

# 4. Install project Python dependencies
pip install -r requirements.txt
```

### Configuration

Copy the default configuration template and update with your environment settings:

```bash
cp config/system_config.h.template config/system_config.h
```

Key parameters to configure in `config/system_config.h`:

```c
// Wi-Fi credentials (for dashboard connectivity)
#define WIFI_SSID          "YOUR_SSID"
#define WIFI_PASSWORD      "YOUR_PASSWORD"

// MQTT broker address (ESP32-C3 dashboard node IP or external broker)
#define MQTT_BROKER_URI    "mqtt://192.168.1.100:1883"

// Accelerometer I2C address
#define ACCEL_I2C_ADDR     0x68   // MPU6050 default; 0x53 for ADXL345

// FreeRTOS task priorities
#define TASK_PRIORITY_SENSOR     5
#define TASK_PRIORITY_INFERENCE  4
#define TASK_PRIORITY_ALERT      3
#define TASK_PRIORITY_DASHBOARD  2

// Inference thresholds
#define HEALTH_SCORE_WARNING     60
#define HEALTH_SCORE_CRITICAL    35
```

### Building & Flashing the ESP32 (Inference Node)

```bash
# Navigate to the inference node firmware directory
cd firmware/inference-node

# Set target to ESP32
idf.py set-target esp32

# Build the firmware
idf.py build

# Flash to device (replace PORT with your serial port, e.g., /dev/ttyUSB0 or COM3)
idf.py -p PORT flash

# Monitor serial output
idf.py -p PORT monitor
```

### Building & Flashing the ESP32-C3 (Dashboard Node)

```bash
# Navigate to the dashboard node firmware directory
cd firmware/dashboard-node

# Set target to ESP32-C3
idf.py set-target esp32c3

# Build and flash
idf.py -p PORT flash monitor
```

### Deploying the Edge Impulse TinyML Model

```bash
# 1. Export your trained model from Edge Impulse as a C++ library
#    (Edge Impulse Studio → Deployment → C++ Library → Build)
#    or use the CLI:
edge-impulse-export --type arduino --out-dir ./model_export

# 2. Copy the exported library into the firmware component directory
cp -r ./model_export/edge-impulse-sdk/ firmware/inference-node/components/ei-model/
cp -r ./model_export/model-parameters/ firmware/inference-node/components/ei-model/
cp -r ./model_export/tflite-model/ firmware/inference-node/components/ei-model/

# 3. Rebuild the firmware to link the updated model
cd firmware/inference-node && idf.py build

# 4. Reflash with the updated model
idf.py -p PORT flash
```

### Accessing the Live Dashboard

Once both nodes are flashed and connected to the same Wi-Fi network:

1. Open a browser and navigate to `http://<ESP32-C3-IP-ADDRESS>/`
2. The dashboard will display real-time machine health scores, vibration trends, and alert logs
3. The ESP32-C3 IP address is printed to the serial monitor on boot

## Project Structure
 
```
rtos-vibration-predictive-maintenance/
├── firmware/
│   ├── inference-node/          # ESP32 main firmware (FreeRTOS + TinyML)
│   │   ├── main/
│   │   │   ├── sensor_task.c    # Task 1: Sensor Acquisition
│   │   │   ├── inference_task.c # Task 2: TinyML Inference
│   │   │   ├── alert_task.c     # Task 3: Alert Handling
│   │   │   ├── dashboard_task.c # Task 4: Dashboard Communication
│   │   │   └── main.c           # FreeRTOS task registration & init
│   │   └── components/
│   │       └── ei-model/        # Edge Impulse C++ SDK & exported model
│   └── dashboard-node/          # ESP32-C3 dashboard firmware
│       └── main/
│           ├── web_server.c     # HTTP + WebSocket server
│           └── dashboard_ui.h   # Embedded HTML/JS dashboard
├── model/
│   ├── training_data/           # Raw vibration dataset (CSV)
│   ├── edge_impulse_project/    # Edge Impulse project export
│   └── model_performance/       # Confusion matrix, accuracy reports
├── config/
│   └── system_config.h.template
├── docs/
│   ├── architecture.md
│   └── wiring_diagram.png
├── scripts/
│   └── data_collection.py       # Host-side data capture utility
└── README.md
```

## Performance Summary

| Metric                       | Value           |
|-----------------------------|-----------------|
| Inference Accuracy           | **89%**         |
| Inference Latency            | **< 76 ms**     |
| Sensor Sampling Rate         | 1 kHz           |
| FreeRTOS Task Count          | 4 concurrent    |
| Alert Response Time          | < 100 ms        |
| Dashboard Update Rate        | ~10 Hz          |
| Model Size (quantized INT8)  | ~45 KB          |
| Peak Inference RAM           | ~28 KB          |
| Target MCU                   | ESP32 / ESP32-C3|


## Contributing

Contributions are welcome. Please open an issue to discuss proposed changes before submitting a pull request. Ensure all firmware code follows the existing FreeRTOS task structure and is validated against the ESP-IDF build system before submission.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature-name`)
3. Commit your changes (`git commit -m 'Add: descriptive commit message'`)
4. Push to the branch (`git push origin feature/your-feature-name`)
5. Open a Pull Request


## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

<p align="center">
  Built with precision for industrial reliability &mdash; where every millisecond and every byte matters.
</p>
