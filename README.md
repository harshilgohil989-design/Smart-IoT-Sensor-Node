# ESP32-C3 IoT Sensor Node (24V Input + Relay Output)



WOKWI PROJECT LINK : https://wokwi.com/projects/479480430286401537




An IoT node that reads temperature and humidity, monitors an isolated 24V digital input, controls a relay (locally and over MQTT), and shows live status on an OLED. The project has two parts: a firmware simulation in Wokwi, and a 2-layer PCB designed in KiCad.

> Internship technical task: Aiothub | IoT

## Features

- DHT22 temperature and humidity read every 3 s
- SSD1306 128x64 OLED: temperature, humidity, input state, relay state, Wi-Fi and MQTT status, uptime
- Input read with an interrupt and software debounce (50 ms), no blocking `delay()` in the main loop
- Relay toggled by the button and remotely over MQTT
- Wi-Fi with automatic, non-blocking reconnect
- JSON telemetry published to a public MQTT broker, with a command topic subscribed
- Sensor failure shows `SENSOR ERR` on the OLED, and the firmware keeps running
- FreeRTOS tasks: sensor, display and network run separately

## Part A: Firmware simulation (Wokwi)

**Simulation link:** `<paste your Wokwi project link here>`

### Hardware used in the simulation

| Part | Purpose |
|---|---|
| ESP32-C3 DevKit | Microcontroller |
| DHT22 | Temperature and humidity |
| SSD1306 OLED, 128x64, I2C | Status display |
| Push button | Simulates the 24V digital input |
| Relay module | Relay output |

### Pin map (ESP32-C3)

| Function | GPIO |
|---|---|
| OLED SDA | 6 |
| OLED SCL | 7 |
| DHT22 data | 3 |
| Input (button, active-low) | 4 |
| Relay driver | 10 |

GPIO2, GPIO8 and GPIO9 are strapping pins and are not used for I/O.

### Wiring

| Part | Part pin | ESP32-C3 |
|---|---|---|
| OLED | VCC / GND / SCL / SDA | 3V3 / GND / IO7 / IO6 |
| DHT22 | VCC / data / GND | 3V3 / IO3 / GND |
| Button | one side / other side | IO4 / GND (internal pull-up) |
| Relay | VCC / GND / IN | 5V / GND / IO10 |

### MQTT

- Broker: `broker.hivemq.com`, port `1883`
- Telemetry topic: `iot/<your-unique-id>/node01/telemetry`
- Command topic: `iot/<your-unique-id>/node01/cmd`

Example telemetry:

```json
{"device":"node01","temp":27.4,"hum":55,"input":1,"relay":0}
```

On sensor failure:

```json
{"device":"node01","error":"sensor","input":0,"relay":0}
```

Commands accepted on the command topic: `ON`, `OFF`, `TOGGLE`, or `{"relay":1}` / `{"relay":0}`.

### How to run

1. Open the Wokwi link above, or create a new ESP32-C3 project and copy `sketch.ino`, `diagram.json` and `libraries.txt` from the `firmware/` folder.
2. Change the topic prefix in `sketch.ino` to a unique value.
3. Press the green play button.
4. Subscribe to the telemetry topic in the HiveMQ web client to see the data.

### Libraries

Adafruit GFX Library, Adafruit SSD1306, DHT sensor library, PubSubClient.

### Test results

| Test | Result |
|---|---|
| Sensor read every 3 s | Pass |
| OLED layout | Pass |
| Button toggles relay (debounced) | Pass |
| Relay control over MQTT | `<Pass / add screenshot>` |
| Sensor failure shows `SENSOR ERR`, no crash | `<Pass / add screenshot>` |
| Wi-Fi reconnect | `<Pass / add screenshot>` |

Screenshots are in the `docs/` folder.

## Part B: PCB design (KiCad)

2-layer board, about 60 x 50 mm, designed in KiCad `<version>`.

### Circuit blocks

| Block | Implementation |
|---|---|
| MCU | ESP32-C3-MINI-1 with 3.3V decoupling, EN reset circuit, BOOT button, USB-C programming |
| 24V input protection | PTC fuse, series Schottky for reverse polarity, SMAJ26A TVS |
| 24V to 5V | LM2596-5.0 buck converter, 33 uH inductor, SS34 catch diode |
| 5V to 3.3V | `<AP2112K-3.3 or LM3940, write the final part>` linear regulator |
| 24V digital input | Series resistor, PC817 optocoupler, 10k pull-up, read by GPIO4 |
| Relay output | 2N7002 MOSFET driver, flyback diode, LED indicator, screw terminal for COM/NO/NC |
| Headers | OLED (I2C with 4.7k pull-ups), DHT22 sensor, UART |
| Mechanical | 4 mounting holes, silkscreen labels for every connector, board name and revision |

### Design decisions

- **Why a buck converter:** a linear regulator from 24V to 3.3V would waste about (24 - 3.3) x 0.15 A, roughly 3 W, as heat. A buck converter avoids that.
- **LDO heat check (5V to 3.3V):** peak (4.6 - 3.3) x 0.35 A is about 0.45 W, which is acceptable for the chosen package.
- **Isolation:** the optocoupler separates the 24V field side from the 3.3V logic, with a creepage gap and separate ground nets (`GND_24V`, `GND`).
- **Antenna keepout:** no copper or components under or near the module antenna.
- **Strapping pins:** GPIO2 and GPIO8 are pulled up, GPIO9 is the BOOT button. They are not used for other signals.
- **TVS note:** the SMAJ26A clamp voltage is close to the buck converter's absolute maximum input. This is acceptable for small surges, and a higher-rated part would add margin.

### Verification

- ERC: 0 errors (screenshot in `docs/erc.png`)
- DRC: 0 errors (screenshot in `docs/drc.png`)
- Remaining warnings: `<none / explain each>`

## Repository structure

```
.
├── README.md
├── firmware/
│   ├── sketch.ino
│   ├── diagram.json
│   └── libraries.txt
├── pcb/
│   ├── kicad/            (project, schematic, PCB files)
│   ├── gerbers/
│   └── bom.csv
├── docs/
│   ├── schematic.pdf
│   ├── pcb-layout.png
│   ├── pcb-3d.png
│   ├── erc.png
│   ├── drc.png
│   └── wokwi-*.png
```

## Known limitations and future work

- The relay is tested in simulation only. A real mains load needs a rated, certified enclosure.
- Possible extensions: OLED page switching with a button, hardware watchdog, deep sleep, OTA updates.

## Author

`<Your name>` | `<email or LinkedIn>`

