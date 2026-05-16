# Zero-Cost Cloud Native: Architecting Hybrid IoT & AI Pipelines with LocalStack

[English](README.md) | [Bahasa Indonesia](README-id.md)

An IoT monitoring project (DHT22) integrated with Machine Learning and AWS simulation using **LocalStack**. The entire infrastructure is designed to run locally without any cloud costs (Zero-Cost).

## 🏗️ Architecture Diagram

```mermaid
graph TD
    classDef edge fill:#e1f5fe,stroke:#0288d1,stroke-width:2px,color:#000
    classDef ui fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px,color:#000
    classDef api fill:#e8f5e9,stroke:#388e3c,stroke-width:2px,color:#000
    classDef aws fill:#fff3e0,stroke:#f57c00,stroke-width:2px,color:#000

    subgraph "📡 Edge Environment"
        ESP32["fas:fa-microchip ESP32<br/>(Sensors: DHT22, Actuators: LED)"]:::edge
    end

    subgraph "💻 Client Interface"
        Dash["fas:fa-desktop Web Dashboard<br/>(Real-time Visualization)"]:::ui
    end

    subgraph "⚙️ Core Application System"
        API["fas:fa-server FastAPI Gateway<br/>(REST & WebSockets)"]:::api
        AI["fas:fa-brain AI Decision Engine<br/>(RandomForest Classifier)"]:::api
        Worker["fas:fa-cogs Async Background Worker"]:::api
    end

    subgraph "☁️ AWS Services (Simulated via LocalStack)"
        SQS["fab:fa-aws SQS<br/>(Event Queue)"]:::aws
        DDB[("fab:fa-aws DynamoDB<br/>(Sensor Logs)")]:::aws
        S3[("fab:fa-aws S3<br/>(ML Model Storage)")]:::aws
        SNS["fab:fa-aws SNS<br/>(Push Notifications)"]:::aws
        CW["fab:fa-aws CloudWatch<br/>(System Metrics)"]:::aws
    end

    %% Connections
    ESP32 -- "1. Sends Telemetry" --> API
    API -- "6. Control Signal (LED)" --> ESP32
    
    Dash <== "2. Live Data Stream" ==> API
    
    API -- "3. Inference Request" --> AI
    AI -. "Loads model (.pkl)" .-> S3
    AI -- "Prediction Result" --> API
    
    API -- "4. Enqueue Data" --> SQS
    SQS -- "Consume Event" --> Worker
    
    Worker -- "5a. Save to Table" --> DDB
    Worker -- "5b. Publish if Alert Triggered" --> SNS
    Worker -- "5c. Push Custom Metrics" --> CW
```

## ✨ Key Features
- **AWS DynamoDB**: Real-time sensor log storage with NoSQL schema.
- **AWS S3**: Storage for ML models (`.pkl`) trained automatically for CI/CD cycles.
- **AWS SNS**: Automated alert system via Topics when temperature exceeds thresholds.
- **AWS SQS**: Event-Driven architecture using message queues for asynchronous data processing.
- **AWS CloudWatch**: Monitoring sensor metrics (Temperature/Humidity) for infrastructure trend analysis.
- **AI Decision Engine**: Environmental condition classification using RandomForest for actuator control (LED).
- **Monitoring Dashboard**: Real-time data visualization with Modern UI & Chart.js.

## 📂 Folder Structure
```text
iot-ai-localstack/
├── backend/            # FastAPI & AWS Logic
│   ├── app/            # Core source code (Business Logic)
│   ├── data/           # Local storage for ML models
│   └── requirements.txt
├── microcontroller/    # ESP32 Code (Arduino/C++)
│   └── esp32/
├── frontend/           # Monitoring Dashboard (HTML/CSS/JS)
├── .gitignore
└── README.md
```

## 🚀 Installation & Usage Guide

### 1. Infrastructure Preparation (LocalStack)
Ensure Docker is running on your system. You will need the LocalStack CLI installed (which requires Python):
```bash
pip install localstack
```
Then, start LocalStack:
```bash
localstack start -d
```
*Services that will be auto-initialized: S3, SQS, SNS, DynamoDB, CloudWatch.*

### 2. Backend Configuration & Startup
Navigate to the `backend` folder, set up the environment, and run the server:

**Windows:**
```powershell
cd backend
python -m venv venv
.\venv\Scripts\python.exe -m pip install -r requirements.txt
.\venv\Scripts\uvicorn.exe app.main:app --reload --host 0.0.0.0
```

**Linux/macOS:**
```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --reload --host 0.0.0.0
```

### 3. Access Dashboard (Frontend)
Open the `frontend/index.html` file in your browser.
> **Tip:** Use the "Live Server" extension in VS Code for a better development experience.

### 4. Microcontroller Setup (ESP32)
1. Open the `microcontroller/esp32/esp32.ino` file in **Arduino IDE**.
2. Install the following required libraries via the Library Manager or download them manually:
   - [DHT sensor library by Adafruit](https://github.com/adafruit/DHT-sensor-library)
   - [WebSockets by Links2004](https://github.com/Links2004/arduinoWebSockets)
   - [ArduinoJson by bblanchon](https://github.com/bblanchon/ArduinoJson)
   - [LiquidCrystal_I2C by johnrickman](https://github.com/johnrickman/LiquidCrystal_I2C)
3. Adjust the following variables:
   - `ssid`: Your WiFi name.
   - `password`: Your WiFi password.
   - `server_ip`: Your Laptop/PC IP (run `ipconfig` or `ifconfig` in terminal to check).
4. Upload the code to your ESP32 board.

## 🛠️ AWS Services Detail

1. **DynamoDB** (NoSQL Database)
   - **Table Name**: `IoT_Sensor_Data` (Partition Key: `id`)
   - **Role**: Serves as the primary storage for all sensor data logs (temperature and humidity) sent by the ESP32. The NoSQL schema allows for flexible and efficient time-series data storage.

2. **S3** (Simple Storage Service)
   - **Bucket Name**: `iot-ai-models`
   - **Role**: Used as object storage for Machine Learning model files (`.pkl`). This model is loaded by the FastAPI backend at startup to perform environmental condition inference based on sensor data.

3. **SNS** (Simple Notification Service)
   - **Topic Name**: `IoT_Alerts`
   - **Role**: A pub/sub service that handles the notification system. If the AI detects abnormal conditions or the temperature exceeds a threshold, the system publishes an alert to this topic, which can be forwarded as an email or SMS.

4. **SQS** (Simple Queue Service)
   - **Queue Name**: `IoT_Sensor_Queue`
   - **Role**: Acts as a message queue to decouple the data ingestion process from storage and notification processes. Data enters SQS first, then is processed asynchronously by the *Background Worker* to keep the API responsive.

5. **CloudWatch** (Monitoring & Metrics)
   - **Namespace**: `IoT/DHT22`
   - **Role**: Collects and monitors custom metrics from the sensors. This is useful for performance tracking, data trend analysis, and monitoring the health of the IoT infrastructure system.

---
Developed with ❤️ by [Muhammad Ikhwan Fathulloh](https://github.com/Muhammad-Ikhwan-Fathulloh)