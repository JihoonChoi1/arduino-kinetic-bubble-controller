# Interactive Bubble Light 🫧💡

> An interactive kinetic art installation that reacts to touch, breath, and presence.

[![Arduino](https://img.shields.io/badge/Arduino-Uno-blue.svg)](https://www.arduino.cc/)
[![Status](https://img.shields.io/badge/Status-Completed-success.svg)]()

## 📖 Overview
**Interactive Bubble Light** is an Arduino-based project that combines lighting, motion, and wind interaction to create a "living" atmosphere. The device changes its behavior based on how the user interacts with it—whether by stroking it, holding it, or blowing into it.

This project focuses on **non-blocking asynchronous control** (using `millis()`) to handle multiple sensors, LED animations, motor control, and a relay simultaneously without any lag.

---

## ✨ Key Features

### 1. Touch Interaction (Reactive Lighting)
- **Sensor:** IR Distance Sensor (Analog A0)
- **Behavior:**
  - **Idle:** Emits a warm White light.
  - **Touch:** Smoothly fades to **Sky Blue** when a hand approaches.
  - **Logic:** Uses a fade-in/out algorithm for organic transitions.

### 2. Breathing Mode (Living Object)
- **Trigger:** Maintain contact ("Hold") for **5 seconds**.
- **Behavior:** The light enters a "Breathing" state, pulsing gently between brightness levels (10 ~ 150).
- **Algorithm:** Uses a `cosine` wave function to create a natural, rhythmic pulsing effect.

### 3. Kinetic Movement (Speed Control)
- **Sensor:** IR Distance Sensor (Analog A5)
- **Actuator:** 360° Continuous Servo Motor
- **Behavior:** The lamp rotates continuously. When an object approaches the sensor, the rotation **slows down**, allowing users to inspect the details.

### 4. Magic Bubble (Wind Interaction) 🫧
- **Sensor:** Sound Sensor (Analog A2)
- **Actuator:** Bubble Machine (Controlled via Relay)
- **Behavior:** Blowing into the sensor ("Hoo!") triggers the bubble machine for **2 seconds**.
- **Stabilization:** Uses a threshold trigger and a non-blocking timer to prevent flickering.

---

## 🛠️ Hardware Requirements

| Component | Description | Quantity |
|-----------|-------------|:--------:|
| **Microcontroller** | Arduino Uno (with Sensor Shield recommended) | 1 |
| **LED** | NeoPixel LED Strip (WS2812B, 60 LEDs) | 1 |
| **Motor** | 360° Continuous Rotation Servo | 1 |
| **Sensors** | Sharp IR Distance Sensor (or compatible) | 2 |
| **Sound Sensor** | Analog Sound Sensor Module (e.g., KY-037) | 1 |
| **Relay** | 5V Relay Module (High/Low Trigger) | 1 |
| **Power** | 5V 3A Power Adapter (Main) & Battery Case (Isolation) | 1 set |
| **Bubble Machine** | Modified for external control | 1 |

---

## 🔌 Pin Configuration

| Component | Arduino Pin | Notes |
|-----------|:-----------:|-------|
| **NeoPixel LED** | `D13` | Data Input |
| **Relay (Bubble)** | `D2` | Active Low/High Configurable |
| **Servo Motor** | `D9` | PWM Control |
| **Touch Sensor** | `A0` | Analog Input |
| **Speed Sensor** | `A5` | Analog Input |
| **Sound Sensor** | `A2` | Analog Input |

---

## 💻 Software Architecture

This project avoids the use of `delay()` to ensure a smooth, lag-free experience.

### 1. Non-Blocking Logic (millis)
Instead of pausing the processor, the code constantly checks the current time (`millis()`) to manage state transitions. This allows the LEDs to fade, the motor to spin, and the bubbles to fire **simultaneously**.

### 2. State Machine
The system separates logic into three distinct phases in the main loop:
1.  **`calculateStateVariables()`**: Reads sensors and determines the logical state (e.g., Is the user holding? Is it time to breathe?).
2.  **`applyVisualEffects()`**: Calculates target colors and brightness based on the state.
3.  **Hardware Output**: Updates the NeoPixels, Servo, and Relay.

### 3. Noise Filtering
- **Input Smoothing:** Implemented `maintenanceTime` buffers to ignore sensor noise or brief signal drops.
- **Power Isolation:** Separate power sources for the Arduino and the inductive loads (Bubble Motor) to prevent electrical noise and resetting.

---

## 🚀 How to Install

1.  Clone this repository.
2.  Open the `.ino` file in the **Arduino IDE**.
3.  Install the required libraries via the Library Manager:
    - `Adafruit NeoPixel`
    - `Adafruit TiCoServo`
4.  Connect the hardware according to the pin configuration.
5.  Upload the code to your Arduino Uno.

---

## 📝 License
This project is open-source and available under the **MIT License**.
