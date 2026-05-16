# Zero-Cost Cloud Native: Architecting Hybrid IoT & AI Pipelines with LocalStack

[English](README.md) | [Bahasa Indonesia](README-id.md)

An IoT monitoring project (DHT22) integrated with Machine Learning and AWS simulation using [LocalStack](https://www.localstack.cloud/). The entire infrastructure is designed to run locally using [AWS](https://aws.amazon.com/) patterns without any cloud costs (Zero-Cost).

## 🏗️ Architecture Diagram

![System Architecture](./images/AIoT_Architecture.png)

## ✨ Key Features
![AWS Services Used](./images/ServiceAWS.png)

- **AWS DynamoDB**: Real-time sensor log storage with NoSQL schema.
- **AWS S3**: Storage for ML models (`.pkl`) trained automatically for CI/CD cycles.
- **AWS SNS**: Automated alert system via Topics when temperature exceeds thresholds.
- **AWS SQS**: Event-Driven architecture using message queues for asynchronous data processing.
- **AWS CloudWatch**: Monitoring sensor metrics (Temperature/Humidity) for infrastructure trend analysis.
- **AI Decision Engine**: Environmental condition classification using RandomForest for actuator control (LED).
- **FreeRTOS Multitasking**: Dual-core execution on ESP32 for zero-lag real-time WebSocket communication and sensor processing.
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
├── images/             # Documentation Assets
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
To stop LocalStack:
```bash
localstack stop
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

### 3. Environment Configuration (.env)
Create a `.env` file in the `backend` folder and fill in the following values:

#### 🔹 LocalStack (AWS)
If you are using **LocalStack Community Edition**, you do not need real AWS credentials. You can use the following default values:
- `AWS_ACCESS_KEY_ID=test`
- `AWS_SECRET_ACCESS_KEY=test`
- `AWS_REGION=us-east-1`
- `LOCALSTACK_ENDPOINT=http://localhost:4566`

#### 🔹 Telegram Bot (Alert Notifications)
To receive real-time critical alerts on your mobile device:
1. **Get Bot Token**: Search for [@BotFather](https://t.me/botfather) on Telegram. Type `/newbot` and follow the steps to receive your `TELEGRAM_BOT_TOKEN`.
2. **Get Chat ID**:
   - Search for [@userinfobot](https://t.me/userinfobot) or [@getmyid_bot](https://t.me/getmyid_bot) on Telegram.
   - Click **Start**.
   - The bot will reply with your **User ID** (e.g., `123456789`). This numeric value is your `TELEGRAM_CHAT_ID`.
3. Add these keys to your `backend/.env` file.

### 4. Access Dashboard (Frontend)
Open the `frontend/index.html` file in your browser.
> **Tip:** Use the "Live Server" extension in VS Code for a better development experience.

### 5. Microcontroller Setup (ESP32)
1. Open the `microcontroller/esp32/esp32.ino` file in **Arduino IDE**.
2. Install the following required libraries via the Library Manager:
   - [DHT sensor library by Adafruit](https://github.com/adafruit/DHT-sensor-library)
   - [WebSockets by Links2004](https://github.com/Links2004/arduinoWebSockets)
   - [ArduinoJson by bblanchon](https://github.com/bblanchon/ArduinoJson)
   - [LiquidCrystal_I2C by johnrickman](https://github.com/johnrickman/LiquidCrystal_I2C)
   - [NocML by Nocturnailed-Community](https://github.com/Nocturnailed-Community/NocML)
3. Adjust the following variables:
   - `ssid`: Your WiFi name.
   - `password`: Your WiFi password.
   - `server_ip`: Your Laptop/PC IP (run `ipconfig` or `ifconfig` in terminal to check).
4. Upload the code to your ESP32 board.

## 🛠️ AWS Services Detail

1. **DynamoDB** (NoSQL Database)
   - **Table Name**: `IoT_Sensor_Data`
   - **Role**: Primary storage for sensor data logs.

2. **S3** (Simple Storage Service)
   - **Bucket Name**: `iot-ai-models`
   - **Role**: Storage for Machine Learning model files (`.pkl`).

3. **SNS** (Simple Notification Service)
   - **Topic Name**: `IoT_Alerts`
   - **Role**: Handles critical alerts via Telegram & LocalStack.

4. **SQS** (Simple Queue Service)
   - **Queue Name**: `IoT_Sensor_Queue`
   - **Role**: Asynchronous message processing queue.

5. **CloudWatch** (Monitoring & Metrics)
   - **Namespace**: `IoT/DHT22`
   - **Role**: Tracks temperature and humidity metrics for infrastructure trend analysis.
   - **View Metrics**: You can view the collected metrics using the AWS CLI:
     ```bash
     aws --endpoint-url=http://localhost:4566 cloudwatch list-metrics --namespace IoT/DHT22
     ```

## 🔗 References & Community
- **LocalStack**: [Official Website](https://www.localstack.cloud/)
- **AWS**: [Official Website](https://aws.amazon.com/)
- **AWS User Group Bandung**: [Community Page](https://bandung.awscommunity.id/)

---
*Developed for the **AWS User Group Bandung** - [bandung.awscommunity.id](https://bandung.awscommunity.id/)*

Developed with ❤️ by [Muhammad Ikhwan Fathulloh](https://github.com/Muhammad-Ikhwan-Fathulloh)