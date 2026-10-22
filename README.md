# 🚨 LoRa-Based Emergency SOS Alert & Monitoring System

### An IoT-Based Emergency Communication and Real-Time Monitoring Solution

An emergency alert and monitoring system built using the **Arduino Nesso N1**, **LoRa wireless communication**, **NEO-6M GPS module**, and a **Python Flask web dashboard**.

The project aims to provide a practical, long-range emergency communication solution for situations where conventional mobile network connectivity may be limited or unavailable.

## 📌 Project Overview

The **LoRa Emergency SOS System** is an IoT-based solution designed to transmit emergency alerts, share location information, and monitor device activity through a centralized web dashboard.

Users can trigger an emergency alert using a physical SOS button on the device. The system processes the emergency event and provides relevant information to the monitoring dashboard.

The Flask-based dashboard allows operators to view active alerts, acknowledge emergencies, manage their status, examine event history, and monitor device telemetry.

## ✨ Key Features

- 🚨 **Hardware SOS Button:** Trigger or cancel emergency alerts using a physical button.
- 📡 **LoRa Communication:** SX1262-based long-range, low-power wireless communication capability.
- 📍 **GPS Location Tracking:** Supports NEO-6M GPS for geographical coordinates.
- 🖥️ **Real-Time Dashboard:** Flask-powered web interface for monitoring emergency activity.
- 🔔 **Emergency Sound Alerts:** Audible notifications when new SOS events arrive.
- ✅ **Alert Acknowledgement:** Operators can acknowledge and manage incoming emergencies.
- 🔄 **Emergency Resolution:** Resolved and cancelled alerts are tracked independently.
- 📊 **Device Telemetry:** Monitor device information, including battery status.
- 📜 **Event Logs:** Maintain records of emergency-related activities.
- 🌐 **Wi-Fi Connectivity:** Supports local-network communication with the monitoring interface.
- 📱 **Touchscreen Hardware:** Uses the Arduino Nesso N1's integrated display.

## 🛠️ Hardware Requirements

| Component | Purpose |
|---|---|
| Arduino Nesso N1 | Main microcontroller and communication platform |
| Integrated SX1262 LoRa | Long-range wireless communication |
| NEO-6M GPS Module | Location acquisition |
| Physical Push Button | SOS activation and cancellation |
| Integrated Touchscreen | Device interface and status |
| Power Supply | Powering the hardware system |
| Computer/Laptop | Hosting the Flask dashboard |

## 💻 Software & Technologies

**Hardware Development**
- Arduino IDE
- Embedded C/C++
- LoRa communication
- GPS integration
- Wi-Fi networking

**Dashboard Development**
- Python 3
- Flask
- HTML
- CSS
- JavaScript

## ⚙️ System Architecture

The project consists of two major components.

### 1. Emergency SOS Device

The Arduino Nesso N1 acts as the primary emergency device.

Its responsibilities include:
- Detecting emergency button interactions.
- Generating SOS and cancellation events.
- Obtaining GPS coordinates when available.
- Handling device communication.
- Reporting relevant device telemetry.
- Updating the local emergency state.

### 2. Emergency Monitoring Dashboard

A Python Flask application provides a centralized interface.

The dashboard is organized into three main sections:

**Live Alerts**
- Displays emergency notifications.
- Allows alert acknowledgement.
- Supports emergency resolution.
- Maintains distinct resolved and cancelled counts.

**Event Logs**
- Records emergency activity.
- Displays SOS and cancellation events.
- Helps review previous incidents.

**Device Telemetry**
- Displays device-related information.
- Includes battery monitoring.
- Helps assess device connectivity and operational status.

## 🔄 Working Principle

1. The user activates SOS using the physical emergency button.
2. The device generates an emergency event.
3. The GPS module provides coordinates when a valid location fix is available.
4. The configured wireless communication system forwards emergency information for monitoring.
5. The Flask dashboard displays the incoming alert.
6. An audible notification informs the dashboard operator.
7. The operator can acknowledge and resolve the emergency.
8. The system updates the alert status and associated counters.
9. When the emergency is resolved or cancelled, the device returns to its normal monitoring state.

## 🚀 Installation & Setup

### Prerequisites

Ensure the following are available:

- Arduino IDE with appropriate Nesso N1 board support.
- Python 3 installed on your computer.
- Required Arduino libraries for the firmware.
- Arduino Nesso N1 hardware.
- A working local Wi-Fi network or hotspot.

### Step 1: Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/YOUR_REPOSITORY.git
cd YOUR_REPOSITORY
```

Replace the placeholders with the actual repository details.

### Step 2: Configure the Hardware

1. Connect the Arduino Nesso N1 to your computer.
2. Open the project's Arduino firmware in Arduino IDE.
3. Install the libraries required by the sketch.
4. Configure the Wi-Fi credentials and applicable communication settings.
5. Connect and configure the GPS module if used.
6. Select the appropriate board and port.
7. Compile and upload the firmware.

### Step 3: Install Python Dependencies

Install Flask:

```bash
python3 -m pip install flask
```

Install any additional dependencies required by the dashboard source code.

### Step 4: Start the Dashboard

Run the Python Flask server from the directory containing `app.py`:

```bash
python3 app.py
```

### Step 5: Access the Dashboard

Open your browser and navigate to:

```text
http://127.0.0.1:5001
```

For access from another device on the same network, use the server computer's local IP address with port `5001`, provided network access is enabled.

## 📍 GPS Location Handling

The system supports the **u-blox NEO-6M GPS module** for determining device location.

- When a valid GPS fix is available, the system uses the measured coordinates.
- When GPS information is unavailable, the prototype can use predefined Palghar coordinates for demonstration purposes.
- Demonstration coordinates must be clearly identified as fallback data and must not be treated as the user's actual emergency location.

## 📊 Emergency Status Management

The dashboard distinguishes between multiple emergency states.

| Status | Description |
|---|---|
| Active | SOS alert has been triggered |
| Acknowledged | Operator has acknowledged the alert |
| Resolved | Emergency has been handled |
| Cancelled | Emergency alert has been cancelled |

Resolved and cancelled incidents are maintained as separate categories to prevent inaccurate emergency statistics.

## 🎯 Project Objectives

- Develop an accessible hardware-based emergency alert device.
- Explore long-range communication using LoRa technology.
- Integrate GPS capabilities into emergency monitoring.
- Build a centralized, real-time monitoring dashboard.
- Implement reliable alert acknowledgement and state management.
- Provide device telemetry and emergency event history.
- Demonstrate the integration of embedded systems and web technologies.

## 🔮 Future Enhancements

- Multi-device LoRa emergency communication networks.
- Improved location tracking and GPS accuracy.
- SMS and email notifications.
- Cloud-based dashboard access.
- Emergency responder assignment.
- Offline alert storage and synchronization.
- Interactive location mapping.
- Mobile application integration.
- Secure device authentication and encrypted communication.

## 👨‍💻 Project Team

**Department of Computer Science and Engineering (Data Science)**

**St. John College of Engineering and Management**

- Daksh Vinay Tandel
- Sambodhi Dinesh Gawai
- Nikhil Prakash Sawant
- Unnati Tare

### Project Guides

- Navajyoti Katela
- Megha Godboley
- Suresh Kancharla
- Sachin Sase

## ⚠️ Disclaimer

This project is an academic IoT prototype developed for educational and demonstration purposes. It is not a certified emergency response or life-safety system. Communication reliability, location accuracy, and alert delivery are not guaranteed.

## 📄 License

No license has been specified. Refer to the repository's license file if one is added.

---

**Built with Arduino, LoRa, GPS and Python Flask.**

*Connecting embedded technology with smarter emergency monitoring.*
