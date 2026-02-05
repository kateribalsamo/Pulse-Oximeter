<img width="244" height="169" alt="image" src="https://github.com/user-attachments/assets/3b77db9c-6fd0-4430-9b2e-e2c2a70a2188" />


https://github.com/user-attachments/assets/9d8bb47d-1ec7-4706-93c8-251ecae84896

# Pulse-Oximeter
Integrated hardware and mobile application prototype that transmits pulse oximeter data via BLE and visualizes real-time heart rate, SpO₂, and IR signals.

## Overview
The developed device functions as a wearable pulse oximeter prototype designed to provide real-time physiological monitoring and user feedback. The embedded OLED display visually indicated each detected heartbeat using an animated heart icon synchronized with sensor pulse detection. The display simultaneously presented heart rate and oxygen saturation (SpO₂) metrics, which were updated at five-second intervals to improve measurement stability and readability.

In addition to physiological monitoring, the system included a battery-style indicator on the OLED screen that visually represented oxygen saturation levels, providing intuitive user status feedback during low SpO₂ conditions. A red LED feedback module was integrated to support guided therapeutic breathing using a 4-7-8 breathing pattern, where LED brightness was modulated to cue inhale, breath hold, and exhale timing.

To demonstrate remote monitoring capability, the system transmitted physiological data via Bluetooth Low Energy to a Flutter-based mobile application, which displayed real-time vital sign data on an iOS device, validating successful hardware, firmware, and mobile software integration.

## System Components
### Hardware
- ESP32 microcontroller used as the primary processing and BLE communication unit  
- MAX30102 optical pulse oximeter sensor for physiological signal acquisition  
- Breadboard and jumper wiring used for initial circuit prototyping and hardware validation
- Soldering tools/equipment used for permanent component integration and wiring
- Prototype PCB board used for soldered circuit integration and device assembly
- OLED display module for local visualization (heart icon, HR, SpO₂, and status indicators)
- Red LED indicator used to provide guided visual feedback for therapeutic breathing exercises during elevated heart rate detection

### Firmware Development

Firmware was developed using the Arduino IDE to manage sensor data acquisition, signal processing, and Bluetooth Low Energy (BLE) transmission.

Key functions include:
- Reading physiological signals from the MAX30102 sensor
- Processing heart rate and SpO₂ measurements
- Transmitting real-time data to the mobile application via BLE
- Driving OLED display feedback and LED breathing guidance

### Mobile Application

A Flutter-based mobile application was developed to receive and visualize physiological data transmitted from the hardware device via Bluetooth Low Energy (BLE).

The application focuses on real-time monitoring of key physiological metrics, including heart rate and oxygen saturation (SpO₂). The interface dynamically updates as new sensor data is received, allowing continuous remote monitoring of vital sign measurements.

The waveform visualization module was implemented as a preliminary proof-of-concept; however, the primary focus of the mobile application was stable and reliable metric display and BLE communication.


  
