# Zero-Cost Cloud Native: Membangun Arsitektur Hybrid IoT & AI Pipeline dengan LocalStack

[English](README.md) | [Bahasa Indonesia](README-id.md)

Proyek pemantauan IoT (DHT22) yang terintegrasi dengan Machine Learning dan simulasi AWS menggunakan [LocalStack](https://www.localstack.cloud/). Seluruh infrastruktur dirancang untuk berjalan secara lokal menggunakan pola [AWS](https://aws.amazon.com/) tanpa biaya cloud (Zero-Cost).

## 🏗️ Diagram Arsitektur

![Arsitektur Sistem](./images/AIoT_Architecture.png)

## ✨ Fitur Utama
![Layanan AWS yang Digunakan](./images/ServiceAWS.png)

- **AWS DynamoDB**: Penyimpanan log sensor real-time dengan skema NoSQL.
- **AWS S3**: Penyimpanan untuk model ML (`.pkl`) yang dilatih otomatis untuk siklus CI/CD.
- **AWS SNS**: Sistem peringatan otomatis melalui Topik saat suhu melebihi batas.
- **AWS SQS**: Arsitektur Event-Driven menggunakan antrean pesan untuk pemrosesan data asinkron.
- **AWS CloudWatch**: Pemantauan metrik sensor (Suhu/Kelembapan) untuk analisis tren infrastruktur.
- **AI Decision Engine**: Klasifikasi kondisi lingkungan menggunakan RandomForest untuk kontrol aktuator (LED).
- **FreeRTOS Multitasking**: Eksekusi dual-core pada ESP32 untuk komunikasi WebSocket real-time tanpa lag dan pemrosesan sensor yang efisien.
- **Monitoring Dashboard**: Visualisasi data real-time dengan UI Modern & Chart.js.

## 📂 Struktur Folder
```text
iot-ai-localstack/
├── backend/            # Logika FastAPI & AWS
│   ├── app/            # Kode sumber inti (Business Logic)
│   ├── data/           # Penyimpanan lokal untuk model ML
│   └── requirements.txt
├── microcontroller/    # Kode ESP32 (Arduino/C++)
│   └── esp32/
├── frontend/           # Dashboard Pemantauan (HTML/CSS/JS)
├── images/             # Aset Dokumentasi
├── .gitignore
└── README.md
```

## 🚀 Panduan Instalasi & Penggunaan

### 1. Persiapan Infrastruktur (LocalStack)
Pastikan Docker berjalan di sistem Anda. Anda memerlukan LocalStack CLI:
```bash
pip install localstack
```
Kemudian, jalankan LocalStack:
```bash
localstack start -d
```
*Layanan yang akan diinisialisasi otomatis: S3, SQS, SNS, DynamoDB, CloudWatch.*

### 2. Konfigurasi & Menjalankan Backend
Masuk ke folder `backend`, siapkan lingkungan, dan jalankan server:

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

### 3. Konfigurasi Lingkungan (.env)
Buat file `.env` di dalam folder `backend` dan isi dengan nilai berikut:

#### 🔹 LocalStack (AWS)
Jika Anda menggunakan **LocalStack Community Edition**, gunakan nilai default berikut:
- `AWS_ACCESS_KEY_ID=test`
- `AWS_SECRET_ACCESS_KEY=test`
- `AWS_REGION=us-east-1`
- `LOCALSTACK_ENDPOINT=http://localhost:4566`

#### 🔹 Telegram Bot (Notifikasi Peringatan)
Untuk menerima peringatan kritis secara real-time di HP Anda:
1. **Dapatkan Bot Token**: Chat dengan [@BotFather](https://t.me/botfather) di Telegram. Ketik `/newbot` untuk mendapatkan `TELEGRAM_BOT_TOKEN`.
2. **Dapatkan Chat ID**:
   - Cari bot [@userinfobot](https://t.me/userinfobot) atau [@getmyid_bot](https://t.me/getmyid_bot).
   - Klik **Start**.
   - Bot akan membalas dengan **User ID** Anda (contoh: `123456789`).
3. Masukkan nilai tersebut ke file `backend/.env`.

### 4. Mengakses Dashboard (Frontend)
Buka file `frontend/index.html` di browser Anda.
> **Tips:** Gunakan ekstensi "Live Server" di VS Code untuk pengalaman yang lebih baik.

### 5. Setup Mikrokontroler (ESP32)
1. Buka file `microcontroller/esp32/esp32.ino` di **Arduino IDE**.
2. Instal library berikut melalui Library Manager:
   - [DHT sensor library oleh Adafruit](https://github.com/adafruit/DHT-sensor-library)
   - [WebSockets oleh Links2004](https://github.com/Links2004/arduinoWebSockets)
   - [ArduinoJson oleh bblanchon](https://github.com/bblanchon/ArduinoJson)
   - [LiquidCrystal_I2C oleh johnrickman](https://github.com/johnrickman/LiquidCrystal_I2C)
   - [NocML oleh Nocturnailed-Community](https://github.com/Nocturnailed-Community/NocML)
3. Sesuaikan variabel berikut:
   - `ssid`: Nama WiFi Anda.
   - `password`: Kata sandi WiFi Anda.
   - `server_ip`: IP Laptop/PC Anda (cek dengan `ipconfig` di terminal).
4. Upload kode ke board ESP32 Anda.

## 🛠️ Detail Layanan AWS

1. **DynamoDB** (Database NoSQL)
   - **Nama Tabel**: `IoT_Sensor_Data`
   - **Peran**: Penyimpanan utama untuk log data sensor.

2. **S3** (Simple Storage Service)
   - **Nama Bucket**: `iot-ai-models`
   - **Peran**: Penyimpanan file model Machine Learning (`.pkl`).

3. **SNS** (Simple Notification Service)
   - **Nama Topik**: `IoT_Alerts`
   - **Peran**: Menangani peringatan kritis melalui Telegram & LocalStack.

4. **SQS** (Simple Queue Service)
   - **Nama Antrean**: `IoT_Sensor_Queue`
   - **Peran**: Antrean pemrosesan pesan asinkron.

5. **CloudWatch** (Pemantauan & Metrik)
   - **Namespace**: `IoT/DHT22`
   - **Peran**: Melacak metrik suhu dan kelembapan.

## 🔗 Referensi & Komunitas
- **LocalStack**: [Situs Resmi](https://www.localstack.cloud/)
- **AWS**: [Situs Resmi](https://aws.amazon.com/)
- **AWS User Group Bandung**: [Halaman Komunitas](https://bandung.awscommunity.id/)

---
*Dikembangkan untuk **AWS User Group Bandung** - [bandung.awscommunity.id](https://bandung.awscommunity.id/)*

Developed with ❤️ by [Muhammad Ikhwan Fathulloh](https://github.com/Muhammad-Ikhwan-Fathulloh)
