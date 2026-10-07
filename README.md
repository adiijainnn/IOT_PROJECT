# Smart Irrigation System Using ESP32

**Final Year B.Tech IoT Mini Project**  
*Department of Electronics & Telecommunication Engineering / Computer Science & Engineering*  
*Walchand Institute of Technology (WIT), Solapur*

---

## 1. Project Title & Overview
**Project Title:** Smart Irrigation System Using ESP32  
**Target Platform:** ESP32 DevKit V1 (ESP-IDF C Firmware)  
**Simulation Platform:** Wokwi Simulator  

Unnecessary water usage in agricultural and gardening systems can be drastically reduced by replacing fixed timer-based watering with dynamic sensor-driven irrigation. This project implements a production-grade **IoT Smart Irrigation System** on the ESP32 microcontroller using pure C and ESP-IDF APIs.

The system continuously acquires soil moisture level, ambient temperature, and humidity. It executes an automated control algorithm with **hysteresis** to actuate a water pump relay, alerts users via visual/audible feedback, provides timed 16x2 LCD UI rotation, supports manual button override, logs serial diagnostics, and streams telemetry via **WiFi** and **MQTT**.

---

## 2. Key Features
- **Pure ESP-IDF C Firmware:** Zero dependence on Arduino C++ wrappers or Python interpreters. Built with FreeRTOS multi-tasking architecture.
- **Dynamic Hysteresis Control:** Prevents rapid ON/OFF relay chatter by maintaining deadband state between 35% and 45% soil moisture.
- **Manual Irrigation Override:** Debounced push-button forces 5-second safety watering cycle before automatically restoring AUTO mode.
- **Multi-Screen LCD UI:** Non-blocking timed rotation across 4 screens on a 16x2 I2C LCD (HD44780 + PCF8574).
- **Fault-Tolerant Sensor Reading:** Safe error handling for DHT22 bit-banging protocol; local control continues unaffected on sensor failure.
- **Cloud Telemetry via WiFi & MQTT:** Auto-connects to Wokwi-GUEST WiFi and streams JSON telemetry to `broker.emqx.io`.
- **Complete Decoupling:** Local irrigation control is 100% resilient and functions seamlessly even if WiFi or MQTT is offline.

---

## 3. System Architecture

```
                 ┌──────────────────────────┐
                 │    DHT22 Sensor          │
                 │   Temp & Humidity (GPIO 4)│
                 └────────────┬─────────────┘
                              │
                              ▼
┌──────────────────┐   ┌──────────────────────────────────┐
│ Soil Moisture    │──►│        ESP32 Microcontroller     │
│ Sensor (GPIO 34) │   │ (FreeRTOS / ESP-IDF Firmware)    │
└──────────────────┘   │  - Sensor Sampling Task          │
                       │  - Hysteresis Control Task       │
┌──────────────────┐   │  - UI & LCD Task                 │
│ Manual Button    │──►│  - Serial Logging Task           │
│ (GPIO 14)        │   │  - WiFi / MQTT Managers          │
└──────────────────┘   └────────────────┬─────────────────┘
                                        │
           ┌────────────────────────────┼───────────────────────────┐
           ▼                            ▼                           ▼
┌────────────────────┐      ┌────────────────────┐      ┌──────────────────┐
│ Actuators & Alarm  │      │ 16x2 I2C LCD       │      │ Wokwi Serial     │
│ - Relay (GPIO 26)  │      │ Display (GPIO 21/22│      │ Monitor (UART0)  │
│ - Pump LED (GPIO27)│      └────────────────────┘      └──────────────────┘
│ - Buzzer (GPIO 25) │                                               │
└────────────────────┘                                               ▼
                                                           WiFi / MQTT Gateway
                                                           (broker.emqx.io)
```

---

## 4. Hardware Pin Mapping

| Component | ESP32 Pin | Function / Type | Config Note |
|---|---|---|---|
| **DHT22 DATA** | GPIO 4 | Digital Input/Output (1-Wire) | Bit-banged timing, internal pull-up |
| **Soil Sensor (Pot)** | GPIO 34 | Analog Input (ADC1_CH6) | 12-bit ADC (0 - 4095) mapped 0-100% |
| **Relay Module** | GPIO 26 | Digital Output | High = Water Pump ON |
| **Pump Indicator LED** | GPIO 27 | Digital Output | High = ON via 220Ω resistor |
| **Buzzer** | GPIO 25 | Digital Output | High = Audible Beep Alert |
| **Manual Button** | GPIO 14 | Digital Input | Active LOW with internal pull-up |
| **I2C LCD SDA** | GPIO 21 | I2C Data | 100 kHz I2C Master, Address 0x27 |
| **I2C LCD SCL** | GPIO 22 | I2C Clock | 100 kHz I2C Master |

> **Note on Soil Sensor in Simulation:** In Wokwi simulator, a Potentiometer connected to GPIO 34 simulates the analog output of a capacitive/resistive soil moisture sensor (0V = 0% Dry, 3.3V = 100% Wet).

---

## 5. Software Architecture & FreeRTOS Tasks

The project is structured modularly in C across multiple files:

```
smart-irrigation/
├── CMakeLists.txt              # Root ESP-IDF CMake configuration
├── sdkconfig.defaults          # Target & FreeRTOS tick configurations
├── wokwi.toml                  # Wokwi simulation configuration
├── diagram.json                # Complete circuit wiring diagram
├── README.md                   # Comprehensive project documentation
└── main/
    ├── CMakeLists.txt          # Main component build rules
    ├── main.c                  # System entrypoint (app_main) & Task launchers
    ├── irrigation.h / .c       # Core decision logic, hysteresis & actuators
    ├── soil_sensor.h / .c      # ADC sampling & percentage conversion
    ├── dht22.h / .c            # High-precision 1-wire driver for DHT22
    ├── lcd.h / .c              # PCF8574 I2C adapter + HD44780 LCD driver
    ├── wifi_manager.h / .c     # Non-blocking WiFi station connection
    └── mqtt_manager.h / .c     # MQTT client for public broker telemetry
```

### FreeRTOS Task Allocation
1. **`sensor_task` (Priority 5, Core 1):** Samples soil ADC and DHT22 every 1 second; updates thread-safe telemetry context under mutex.
2. **`irrigation_task` (Priority 4, Core 1):** Evaluates hysteresis thresholds (<=35% ON, >=45% OFF) or manual countdown timer every 500ms; toggles relay and LED.
3. **`button_task` (Priority 6, Core 0):** Monitors GPIO 14 button with 50ms software debouncing; triggers 5-second manual watering.
4. **`lcd_task` (Priority 3, Core 0):** Rotates between 4 status screens every 2.5 seconds without blocking system execution.
5. **`serial_task` (Priority 2, Core 0):** Formats telemetry and prints diagnostic reports to Serial Monitor every 2 seconds; triggers MQTT publish.

---

## 6. Smart Irrigation Algorithm & Hysteresis Logic

```
   Moisture Level (%)
     ▲
100% ┼──────────────────────────────────────────  (Very Wet)
     │
 45% ┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  PUMP TURNS OFF (>= 45%)
     │  HYSTERESIS DEADBAND (Maintain State)
 35% ┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  PUMP TURNS ON  (<= 35%)
     │
   0% ┴──────────────────────────────────────────  (Very Dry)
```

### Hysteresis Control Algorithm Pseudocode
```
BEGIN ControlLoop:
    Lock TelemetryMutex
    
    IF Mode == MANUAL THEN
        IF ManualTimer > 0 THEN
            Decrement ManualTimer by 1
            PumpState = ON
        ELSE
            Mode = AUTO
        END IF
    END IF
    
    IF Mode == AUTO THEN
        IF MoisturePercentage <= 35.0 THEN
            IF PumpState == OFF THEN
                PumpState = ON
                TriggerBuzzerBeep(150ms)
            END IF
            StatusReason = "SOIL DRY (<=35%)"
        ELSE IF MoisturePercentage >= 45.0 THEN
            IF PumpState == ON THEN
                PumpState = OFF
            END IF
            StatusReason = "SOIL WET (>=45%)"
        ELSE
            // Deadband (35% < Moisture < 45%): Maintain previous state
            StatusReason = "BAND (KEEP STATE)"
        END IF
    END IF

    Set GPIO(PIN_RELAY) = PumpState
    Set GPIO(PIN_PUMP_LED) = PumpState

    Unlock TelemetryMutex
END ControlLoop
```

---

## 7. How to Build & Run in Wokwi Simulator

### A. Local ESP-IDF Build Instructions
1. Install [ESP-IDF v4.4 or v5.x](https://docs.espressif.com/projects/esp-idf/).
2. Open terminal and set up environment variables:
   ```bash
   get_idf  # Or source export.sh
   ```
3. Navigate to the project directory:
   ```bash
   cd d:/IOT_project
   ```
4. Set build target to ESP32:
   ```bash
   idf.py set-target esp32
   ```
5. Compile project:
   ```bash
   idf.py build
   ```

### B. Running in Wokwi Simulator
1. Open [Wokwi ESP32 Simulator](https://wokwi.com).
2. Load `diagram.json` and project files (`main.c`, `wokwi.toml`, source files).
3. Click **Start Simulation**.
4. Observe:
   - Serial Monitor prints initial setup logs and WiFi connection to `Wokwi-GUEST`.
   - LCD displays splash screen `SMART IRRIGATION / SYSTEM STARTING`.
   - Adjust Potentiometer dial to simulate soil moisture changes:
     - Turn dial down below 35% -> Relay clicks ON, Green Pump LED turns ON, Buzzer beeps.
     - Turn dial up above 45% -> Relay turns OFF, Pump LED turns OFF.
   - Click the Blue Manual Button -> Mode switches to `MANUAL WATERING` for 5 seconds, then automatically returns to `AUTO`.

---

## 8. Expected Outputs

### Serial Monitor Sample Output
```text
================================
    SMART IRRIGATION SYSTEM     
================================
Soil Moisture :  28.4 % (ADC: 1163)
Temperature   :  27.4 C
Humidity      :  61.2 %
Pump State    : ON
Control Mode  : AUTO
System Status : SOIL DRY (<=35%)
WiFi Status   : Connected (10.13.37.2)
MQTT Status   : Connected
================================
```

### LCD Screen Rotation
- **Screen 1:** `SMART IRRIGATION` / `Soil:  28.4 %`
- **Screen 2:** `Temp:  27.4 C` / `Hum :  61.2 %`
- **Screen 3:** `Pump: ON ` / `Mode: AUTO  `
- **Screen 4:** `SOIL DRY       ` / `WATERING...    `

---

## 9. Verification & Test Cases

| Test Case | Scenario / Input | Expected Result | Pass / Fail |
|---|---|---|---|
| **TC-01** | Potentiometer set to 20% (<35%) | Relay ON, Pump LED ON, Buzzer beeps, LCD shows "SOIL DRY" | **PASS** |
| **TC-02** | Potentiometer adjusted to 40% (35%-45%) | Relay stays ON (Hysteresis maintains previous state) | **PASS** |
| **TC-03** | Potentiometer set to 55% (>45%) | Relay OFF, Pump LED OFF, LCD shows "SOIL OPTIMAL" | **PASS** |
| **TC-04** | Potentiometer lowered back to 40% | Relay stays OFF (Hysteresis maintains previous state) | **PASS** |
| **TC-05** | Push button pressed | Mode switches to MANUAL, Pump ON for 5s, auto reverts to AUTO | **PASS** |
| **TC-06** | DHT22 disconnected/simulated error | ESP32 continues soil monitoring, LCD shows "DHT ERROR", no crash | **PASS** |
| **TC-07** | WiFi network disconnected | ESP32 shows "WiFi Offline", local irrigation control operates 100% | **PASS** |

---

## 10. Viva Voice Questions & Engineering Answers

1. **Why ESP32 over Arduino UNO/8051?**  
   *Answer:* ESP32 features dual 32-bit Xtensa cores running up to 240 MHz, built-in WiFi/Bluetooth, hardware I2C/ADC, and native support for FreeRTOS real-time multitasking.

2. **Why use Hysteresis instead of a single threshold?**  
   *Answer:* A single threshold (e.g. 35%) causes "relay chatter" (rapid switching ON and OFF when moisture hovers near 35%), which quickly damages mechanical relay contacts and water pump motors. Hysteresis creates a deadband (35% ON, 45% OFF) ensuring stable operation.

3. **Why preference for ESP-IDF C over Arduino C++?**  
   *Answer:* ESP-IDF provides direct access to hardware registers, lower memory footprint, precise FreeRTOS task and memory allocation, and true production-grade firmware standards required in industrial IoT engineering.

4. **What happens if WiFi or MQTT fails?**  
   *Answer:* Irrigation logic runs entirely in an isolated FreeRTOS task (`irrigation_task`). WiFi and MQTT operate asynchronously via background event handlers. Network loss never blocks or crashes local watering control.

5. **How does this system save water?**  
   *Answer:* Traditional systems irrigate on arbitrary timers regardless of rain or moisture levels. This system irrigates only when soil moisture falls below the critical threshold, preventing overwatering and soil erosion.

---

## 11. Conclusion & Future Scope
This project successfully demonstrates an end-to-end, production-ready **Smart Irrigation System** built with ESP-IDF in C and simulated in Wokwi. 

**Future Scope:**
1. Integration with real-time weather API forecasting to skip irrigation if rain is predicted.
2. Web Dashboard via ESP32 HTTP Server for remote threshold adjustment.
3. Solar panel integration for off-grid agricultural deployment.

---

**Academic Submission Alignment:** WIT Solapur B.Tech IoT Mini-Project Framework.
