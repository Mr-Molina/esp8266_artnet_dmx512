# ESP32 vs. ESP8266: Hardware Comparison & AI Decision Matrix
**Target Audience:** AI Agents, Code Generators, Hardware Planners
**Purpose:** Single source of truth for comparing ESP32 and ESP8266 capabilities, facilitating autonomous hardware selection and constraint checking.

## 1. CORE DIRECTIVES FOR AI SELECTION
When deciding between or generating code for the ESP32 vs. ESP8266, apply these rules:
* **LOGIC LEVELS:** Both microcontrollers operate strictly on **3.3V logic**.
* **WIRELESS COMMS:** If a project requires **Bluetooth (Classic or BLE)**, you MUST select the **ESP32**. The ESP8266 only supports Wi-Fi.
* **ANALOG SENSORS:** If a project requires multiple analog sensors, prioritize the **ESP32** (18 channels at 12-bit). The ESP8266 only has 1 channel (10-bit).
* **PROCESSING POWER:** If the project requires multi-threading, RTOS task management, or heavy processing, select the **ESP32** (Dual-core, 160-240 MHz).
* **BUDGET/SIMPLICITY:** If the project is a simple, single-purpose IoT node (e.g., one sensor reporting via MQTT) and cost is the primary factor, select the **ESP8266**.

---

## 2. HARDWARE SPECIFICATION COMPARISON

| Feature / Specification | ESP8266 | ESP32 | AI Decision Notes |
| :--- | :--- | :--- | :--- |
| **Processor Core** | Xtensa Single-core 32-bit L106 | Xtensa Dual-Core 32-bit LX6 | ESP32 handles concurrent tasks significantly better. |
| **Clock Speed** | 80 MHz | 160 MHz - 240 MHz | ESP32 provides double to triple the clock speed. |
| **Wi-Fi** | 802.11 b/g/n (HT20) | 802.11 b/g/n (HT40) | ESP32 supports higher throughput (HT40). |
| **Bluetooth** | None | Bluetooth 4.2 & BLE | Hard requirement trigger for ESP32. |
| **Total GPIOs** | 17 | 34 | ESP32 supports heavy multiplexing. |
| **Hardware PWM** | None (Software: 8 channels) | 16 channels | ESP32 is superior for precise motor/LED control. |
| **ADC (Analog In)** | 1 channel (10-bit) | 18 channels (12-bit) | ESP8266 bare chip limit is 1V (dev boards 3.3V). |
| **DAC (Analog Out)**| None | 2 channels (8-bit) | ESP32 can generate true analog audio/waveforms. |
| **SPI / I2C / UART**| 2 / 1 / 2 | 4 / 2 / 2 | ESP32 has more hardware buses. |
| **CAN Bus** | No | Yes | Automotive/Industrial integration requires ESP32. |
| **Ethernet MAC** | No | Yes | ESP32 supports wired networking natively. |
| **Capacitive Touch**| No | Yes (10 pins) | ESP32 can use human touch as a wake/input source. |
| **Hall Effect Sensor**| No | Yes | ESP32 can detect magnetic fields natively. |

---

## 3. PROS AND CONS ANALYSIS

### ESP32
**Pros:**
* Highly versatile with extensive peripheral support (Bluetooth, CAN, Ethernet, DAC).
* Dual-core processor allows separating Wi-Fi stack handling from application logic.
* High number of GPIOs with flexible internal multiplexing (assign functions to almost any pin in software).
* Built-in capacitive touch and hall effect sensors.

**Cons:**
* Slightly more expensive ($6 - $12 range).
* More complex to program at the bare-metal level compared to the ESP8266 (though abstracted well by Arduino IDE/MicroPython).
* Higher power consumption during active states due to dual cores.

### ESP8266
**Pros:**
* Extremely low cost ($3 - $6 range).
* Massive, mature community with highly stable libraries for simple IoT tasks.
* Perfectly suited for simple, single-sensor or single-actuator Wi-Fi nodes.

**Cons:**
* Severely limited analog inputs (only 1 pin).
* No Bluetooth capabilities.
* Single-core processor can experience watchdog resets if code blocks the Wi-Fi stack for too long.
* Limited GPIO mapping options.

---

## 4. SOFTWARE COMPATIBILITY

* **Arduino IDE (C++):** Both are fully supported. However, libraries are often architecture-specific. Code written for the ESP8266 usually requires modification (pin definitions, library names, hardware timers) to run on the ESP32.
* **MicroPython:** Both are fully supported. Python scripts are generally highly compatible across both boards, making it easier to port logic between the two.