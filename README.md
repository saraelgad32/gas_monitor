# IoT-Based Gas Monitoring System

An IoT-based gas monitoring system using ESP32, MQ-5 gas sensor, and DHT11 temperature and humidity sensor. The system collects environmental measurements, transmits data through MQTT, stores measurements in an SQLite database, and displays them through a web dashboard.

## Project Architecture

The system consists of four main parts:

1. **ESP32 Firmware:** Reads gas, temperature, and humidity measurements, manages the alarm state and buzzer, and communicates through Wi-Fi and MQTT.
2. **MQTT Communication:** Mosquitto MQTT broker handles telemetry and remote control messages.
3. **Backend:** Python services using Flask, Paho MQTT, and SQLite process and store sensor measurements.
4. **Web Dashboard:** Displays measurements and provides remote buzzer mute functionality.

## Hardware Components

- ESP32 development board
- MQ-5 gas sensor
- DHT11 temperature and humidity sensor
- Buzzer
- Connecting wires

## Technologies Used

- Arduino / ESP32
- C++
- Python
- Flask
- MQTT
- Mosquitto
- Paho MQTT
- SQLite
- HTML
- CSS
- JavaScript

## Project Structure

```text
gas_monitor/
├── backend/
│   ├── app.py
│   ├── database.py
│   ├── mqtt_client.py
│   ├── requirements.txt
│   ├── templates/
│   │   └── index.html
│   └── static/
│       ├── css/
│       │   └── style.css
│       └── js/
│           └── dashboard.js
├── esp32/
│   └── ESP32_Gas_Monitor_Remote_Mute.ino
├── .gitignore
└── README.md
```

## MQTT Topics

| Topic | Purpose |
|---|---|
| `gasmonitor/esp32/telemetry` | ESP32 publishes sensor measurements and system status |
| `gasmonitor/esp32/command` | ESP32 receives remote control commands |

## Backend API

| Endpoint | Method | Description |
|---|---|---|
| `/` | GET | API status |
| `/dashboard` | GET | Web dashboard |
| `/api/latest` | GET | Retrieve the latest measurement |
| `/api/history` | GET | Retrieve measurement history |
| `/api/stream` | GET | Stream new measurements using Server-Sent Events |
| `/api/buzzer/mute` | POST | Send a remote mute command through MQTT |

## Installation and Configuration

### 1. Clone the repository

```bash
git clone <YOUR_GITHUB_REPOSITORY_URL>
cd gas_monitor
```

### 2. Install backend dependencies

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 3. Configure the ESP32

Open the Arduino sketch:

`esp32/ESP32_Gas_Monitor_Remote_Mute.ino`

Configure your Wi-Fi credentials and MQTT broker address locally before uploading the firmware to the ESP32.

### 4. Start the MQTT broker

Install and start Mosquitto on your system.

```bash
sudo systemctl start mosquitto
```

### 5. Start the MQTT subscriber

From the `backend` directory, with the virtual environment activated:

```bash
python mqtt_client.py
```

### 6. Start the Flask application

Open another terminal:

```bash
cd ~/gas_monitor/backend
source venv/bin/activate
python app.py
```

### 7. Open the dashboard

Visit:

`http://localhost:5000/dashboard`

## Database

Sensor measurements are stored in an SQLite database named `gas_monitor.db`.

Stored information includes:

- Timestamp
- Gas sensor raw value
- Temperature
- Humidity
- Gas status
- Gas detection state
- Alarm latch state
- Buzzer state
- Buzzer mute state

## Remote Buzzer Control

The Flask backend provides an API endpoint that publishes a `MUTE` command to the MQTT command topic. The ESP32 receives the command and handles buzzer control.

## Project Status

Developed as an IoT project integrating embedded systems, MQTT communication, backend development, database management, and web-based monitoring.

## Safety Notice

This project is intended for educational and experimental purposes. It is not a certified gas detection or life-safety system.
