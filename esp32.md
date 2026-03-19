# ESP32 Microcontroller Hardware Context & Pinout Reference
**Target Audience:** AI Agents, Code Generators, Hardware Compilers
**Purpose:** Single source of truth for ESP32 (specifically ESP-WROOM-32 based) GPIO capabilities, constraints, and peripheral mapping.

## 1. CORE DIRECTIVES FOR AI AGENTS
When generating wiring diagrams, C++/MicroPython code, or hardware advice for ESP32, you MUST obey these rules:
* **LOGIC LEVELS:** The ESP32 is strictly **3.3V logic**. It is **NOT** 5V tolerant. Applying 5V to any GPIO will destroy the chip.
* **WIFI + ADC CONFLICT:** ADC2 pins CANNOT be used to read analog inputs while the Wi-Fi radio is active. Always assign analog sensors to ADC1 if Wi-Fi is required.
* **INPUT-ONLY PINS:** GPIOs 34, 35, 36, and 39 cannot output digital signals and lack internal pull-up/pull-down resistors.
* **SPI FLASH PINS:** Never assign GPIOs 6, 7, 8, 9, 10, or 11. They are internally connected to the SPI flash.
* **CURRENT LIMITS:** Absolute maximum current drawn per GPIO is **40mA**. Recommend 20mA for continuous loads.

---

## 2. GPIO MASTER CLEARANCE TABLE

| GPIO | Input | Output | AI Assignment Directive | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **0** | Pulled up | OK | **CAUTION** | Outputs PWM at boot. Must be LOW to enter flashing mode. |
| **1** | TX0 | OK | **CAUTION** | Debug output at boot. Avoid using if Serial Monitor is needed. |
| **2** | OK | OK | **CAUTION** | Must be floating/LOW during boot to flash. Connected to on-board LED. |
| **3** | OK | RX0 | **CAUTION** | HIGH at boot. Avoid using if Serial Monitor is needed. |
| **4** | OK | OK | **SAFE** | |
| **5** | OK | OK | **CAUTION** | Outputs PWM at boot. Strapping pin (Must be HIGH during boot). |
| **6-11** | NO | NO | **BANNED** | Connected to integrated SPI flash. DO NOT USE. |
| **12** | OK | OK | **CAUTION** | Boot fails if pulled high. Strapping pin. |
| **13** | OK | OK | **SAFE** | |
| **14** | OK | OK | **CAUTION** | Outputs PWM signal at boot. |
| **15** | OK | OK | **CAUTION** | Outputs PWM at boot. Strapping pin (Must be HIGH during boot). |
| **16** | OK | OK | **SAFE** | |
| **17** | OK | OK | **SAFE** | |
| **18** | OK | OK | **SAFE** | |
| **19** | OK | OK | **SAFE** | |
| **21** | OK | OK | **SAFE** | Default I2C SDA |
| **22** | OK | OK | **SAFE** | Default I2C SCL |
| **23** | OK | OK | **SAFE** | |
| **25** | OK | OK | **SAFE** | DAC1 |
| **26** | OK | OK | **SAFE** | DAC2 |
| **27** | OK | OK | **SAFE** | |
| **32** | OK | OK | **SAFE** | |
| **33** | OK | OK | **SAFE** | |
| **34** | OK | NO | **INPUT ONLY** | No internal pull resistors. |
| **35** | OK | NO | **INPUT ONLY** | No internal pull resistors. |
| **36** | OK | NO | **INPUT ONLY** | No internal pull resistors. Also known as VP. |
| **39** | OK | NO | **INPUT ONLY** | No internal pull resistors. Also known as VN. |

---

## 3. PERIPHERAL MAPPING

The ESP32 utilizes an internal multiplexer, allowing many peripherals to be routed to any capable GPIO. However, defaults exist.

### 3.1 I2C (Inter-Integrated Circuit)
* **Hardware Channels:** 2
* **Default SDA:** GPIO 21
* **Default SCL:** GPIO 22
* *Note: Can be remapped in software `Wire.begin(SDA_PIN, SCL_PIN);`*

### 3.2 SPI (Serial Peripheral Interface)
* **Hardware Channels:** 3 (SPI, HSPI, VSPI). *SPI is reserved for internal flash.*

| SPI Bus | MOSI | MISO | CLK (SCK) | CS (SS) |
| :--- | :--- | :--- | :--- | :--- |
| **VSPI (Default)** | GPIO 23 | GPIO 19 | GPIO 18 | GPIO 5 |
| **HSPI** | GPIO 13 | GPIO 12 | GPIO 14 | GPIO 15 |

### 3.3 UART (Serial Communication)
* **UART0:** GPIO 1 (TX), GPIO 3 (RX) - *Reserved for Flashing / Serial Monitor.*
* **UART1:** GPIO 10 (TX), GPIO 9 (RX) - *Connected to SPI Flash. MUST BE REMAPPED to use.*
* **UART2:** GPIO 17 (TX), GPIO 16 (RX) - *Safe to use.*

### 3.4 ADC (Analog-to-Digital Converter)
* **Resolution:** 12-bit (0-4095 corresponds to 0V-3.3V).
* **Linearity Note:** Behavior is non-linear at the extreme ends (0-0.1V and 3.2-3.3V).
* **ADC1 (Safe with Wi-Fi):** GPIOs 32, 33, 34, 35, 36, 37, 38, 39.
* **ADC2 (Fails with Wi-Fi):** GPIOs 0, 2, 4, 12, 13, 14, 15, 25, 26, 27.

### 3.5 DAC (Digital-to-Analog Converter)
* **Resolution:** 8-bit.
* **Channels:** DAC1 (GPIO 25), DAC2 (GPIO 26).

### 3.6 PWM (Pulse Width Modulation)
* **Channels:** 16 independent channels.
* **Availability:** Can be routed to ANY pin capable of acting as an Output.

### 3.7 Capacitive Touch GPIOs
* Internal sensors detect electrical charge variations (like human skin).
* **Pins:** GPIOs 0, 2, 4, 12, 13, 14, 15, 27, 32, 33.

### 3.8 RTC GPIOs (Deep Sleep Wake-up)
* Pins routed to the RTC low-power subsystem. Required to wake ESP32 from Deep Sleep using external triggers.
* **Pins:** GPIOs 0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 39.

---

## 4. BOOT & STRAPPING PIN BEHAVIORS

The ESP32 reads the state of specific pins at power-on to determine its boot mode (e.g., normal execution vs. serial flashing). Attaching peripherals that pull these pins in the wrong direction will cause the device to fail to boot.

| GPIO | Required State at Boot | Consequence of Failure |
| :--- | :--- | :--- |
| **0** | HIGH (Normal) / LOW (Flash) | Will not enter flash mode / Will not boot code |
| **2** | Floating or LOW | Will not enter flash mode |
| **5** | HIGH | Boot configuration failure |
| **12** | LOW | Boot fails (Flash voltage incorrectly set to 1.8V instead of 3.3V) |
| **15** | HIGH | Boot configuration failure |

*Additional Boot States:*
* **PWM Emitted at Boot:** GPIOs 0, 5, 14, 15. Do not connect to relays or motors without hardware pull-down buffers.
* **HIGH at Boot:** GPIO 1, 3.

## 5. HARDWARE MISCELLANEOUS
* **EN Pin:** Enable pin for the 3.3V regulator. Pulled HIGH internally. Connect to GND to restart the ESP32.
* **Hall Effect Sensor:** Built into the silicon to detect magnetic fields.