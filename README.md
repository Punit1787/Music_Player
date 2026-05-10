# 🎵 ESP32 Bluetooth Audio Player

A hardware + software mini project — a Bluetooth audio player built on the ESP32 microcontroller that streams WAV files from an SD card to a Bluetooth speaker, with an LCD display and physical button controls.

## 📦 Hardware Components

- ESP32 Development Board
- 16x2 LCD Display (I2C, address 0x27)
- SD Card Module (SPI)
- Push Buttons × 5 (Play/Pause, Next, Prev, Vol+, Vol-)
- Breadboard, jumper wires, resistors

## 🔌 Pin Configuration

| Component | Pin |
|---|---|
| SD Card CS | GPIO 5 |
| SPI SCK | GPIO 18 |
| SPI MISO | GPIO 19 |
| SPI MOSI | GPIO 23 |
| BTN Play | GPIO 12 |
| BTN Prev | GPIO 13 |
| BTN Next | GPIO 14 |
| BTN Vol+ | GPIO 32 |
| BTN Vol- | GPIO 33 |
| LCD SDA/SCL | I2C default |

## 🛠️ Software / Libraries

- `BluetoothA2DPSource` — Streams audio to BT speaker via A2DP
- `LiquidCrystal_I2C` — LCD control
- `SD.h` + `SPI.h` — SD card file I/O

## ⚙️ How It Works

1. ESP32 initializes SD card and mounts the `/Music_ESP` folder
2. Connects to the target Bluetooth speaker by name (`ZEB-NOBLE+`)
3. Reads `.WAV` files (PCM 16-bit) into an 8KB ring buffer
4. Audio callback streams samples to the A2DP stack in real time
5. LCD scrolls the song name on row 0; row 1 shows Play/Pause status and volume %

## 📁 Music Folder

Place `.WAV` files (PCM, 16-bit, mono/stereo) inside `/Music_ESP` on the SD card root.

## 🚀 Setup

1. Install Arduino IDE + ESP32 board support
2. Install required libraries via Library Manager
3. Set `SPEAKER_NAME` in code to your Bluetooth speaker's name
4. Upload `main.ino` to the ESP32
5. Power on — it will auto-connect and play

## 👨‍💻 Team

Punit Saini , Saish Patil , Tanishk Mahajan, Shubham Sahani, Pranav Arjugade
Vidyalankar Institute of Technology — Semester II, 2025–26
