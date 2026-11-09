# Firmware demos for the Pomodoro timer

Nine small Arduino sketches. Each one tests one part of the board, so you can
check the hardware step by step and then run the full timer.

**Status:** written and checked for typos against stand-in headers, but **not yet
run on the real board**. If something fails to compile or behaves oddly, take a
screenshot of the error or the screen. That is good journal material, and it
tells me exactly what to fix.

## 1. One-time setup (Arduino IDE 2)

1. **Board package:** Tools > Board > Boards Manager, search `esp32`, install
   **"esp32 by Espressif Systems"** (version **3.0.0 or newer**).
2. **Board:** Tools > Board > esp32 > **XIAO_ESP32C3**.
3. **USB CDC On Boot:** Tools > USB CDC On Boot > **Enabled**.
   Without this you won't see anything in the Serial Monitor.
4. **Library:** Sketch > Include Library > Manage Libraries, search `U8g2`,
   install **"U8g2" by oliver**.
5. **Port:** plug in USB, then Tools > Port and pick the XIAO.
6. Open a sketch (File > Open, pick the `.ino`), then press **Upload** (the arrow).

Tips:
- Every sketch is a single `.ino` file. When you open one, the Arduino IDE asks to
  put it in a folder with the same name. Say **OK**. It moves the file into that
  folder, so work on a copy if you want to keep this folder as it is.
- Sketches 06 and 09 contain the orientation code inside the file, so nothing else is needed.
- If upload fails: hold the small **BOOT** button on the XIAO while plugging in the
  USB cable, then upload again.
- You can flash over USB with the power switch off.
- Serial Monitor: Tools > Serial Monitor, set the speed to **115200**.

## 2. Pin map (from the schematic)

| Pin | Name | What is connected |
|---|---|---|
| D1 | `BAT_SENSE` | battery divider (100k / 33k) |
| D2 | `BUZZER` | buzzer transistor |
| D3 | `MPU_INT` | MPU6050 interrupt |
| D4 | `SDA` | I2C data (OLED and MPU) |
| D5 | `SCL` | I2C clock (OLED and MPU) |
| D6 | `BTN1` | button 1 (to GND) |
| D7 | `BTN2` | button 2 (to GND) |
| D10 | `LED1` | status LED |

## 3. The sketches

| # | File | What it shows you |
|---|---|---|
| 01 | `01_hello_pins.ino` | Serial output, blinking LED, button presses |
| 02 | `02_i2c_scanner.ino` | Finds the OLED (0x3C) and the MPU6050 (0x68) |
| 03 | `03_oled_basics.ino` | Text, shapes, progress bars, rotation, big clock, brightness |
| 04 | `04_oled_animations.ino` | Ball, spinner, countdown ring, scrolling text, tomato, sine wave |
| 05 | `05_mpu_config.ino` | MPU6050 settings: ranges, filter, sample rate, motion interrupt, bubble level, Serial Plotter |
| 06 | `06_auto_rotate.ino` | Calibration wizard, screen rotates with the timer, face-down detection |
| 07 | `07_buzzer_buttons_led.ino` | Melodies, short/long button presses, LED patterns |
| 08 | `08_battery_monitor.ino` | Battery voltage and percentage, low-battery warning |
| 09 | `09_pomodoro_demo.ino` | The whole timer: countdown, sessions, rotation, flip-to-start, sound, battery |

Do them **in order**. If 02 doesn't find the OLED and MPU, nothing after it will work.

## 4. Plan for the journals (4 journals)

Take a screenshot (or a phone photo) after each step. Save the pictures in the
journal's image folder: `images/journal-11` to `images/journal-14`.
Send me the folder and I'll rename the files and write the notes, like before.

### Journal 11: setting up and first signs of life
Sketches: **01, 02, 03**

| Picture to take | From |
|---|---|
| Boards Manager with the esp32 package installed | setup step 1 |
| Tools menu showing `XIAO_ESP32C3` and `USB CDC On Boot: Enabled` | setup steps 2 and 3 |
| Library Manager showing U8g2 installed | setup step 4 |
| Arduino IDE with sketch 01 open and "Done uploading" | 01 |
| Serial Monitor showing the pin map and button messages | 01 |
| Serial Monitor showing `found device at 0x3C` and `0x68` | 02 |
| Photo of the OLED on two or three different pages (text, shapes, rotation) | 03 |
| Any error message you hit and how you fixed it | any |

### Journal 12: animations, sound, buttons and LED
Sketches: **04, 07**

| Picture to take | From |
|---|---|
| Photos of 3 animations (for example ball, ring, tomato) | 04 |
| Photo of the OLED showing the button boxes filled in | 07 |
| Serial Monitor showing "Playing: ..." and "LED pattern: ..." | 07 |
| Photo or short video of the LED pattern (heartbeat or breathing) | 07 |
| Arduino IDE with the sketch open | 04 and 07 |

### Journal 13: motion sensor and auto-rotate
Sketches: **05, 06**

| Picture to take | From |
|---|---|
| Serial Monitor with the readable output (acc, gyro, roll, pitch) | 05 |
| Tools > Serial Plotter after typing `p` and moving the board | 05 |
| Photo of the OLED in numbers view and bubble view | 05 |
| Serial Monitor after changing a setting (`a`, `d` or `m`) | 05 |
| Photos of the calibration wizard steps on the OLED | 06 |
| Photos of the screen staying upright when the timer is turned 90 degrees and upside down | 06 |
| Photo of the "FLIPPED" screen | 06 |

### Journal 14: battery and the full Pomodoro demo
Sketches: **08, 09**

| Picture to take | From |
|---|---|
| Photo of the battery screen with a multimeter reading next to it | 08 |
| Serial Monitor with the battery voltages | 08 |
| Photos of the timer in FOCUS, BREAK and LONG BREAK | 09 |
| Photos of the timer in portrait (turned 90 degrees) | 09 |
| Photo of the 4 dots after finishing sessions | 09 |
| The finished board in its case, running | 09 |

## 5. Things you may need to adjust

- **Rotates the wrong way (06 and 09):** swap `U8G2_R1` and `U8G2_R3` in the `rotationFor` function.
- **Battery voltage is off (08 and 09):** measure the battery with a multimeter and change `CALIBRATION` in 08 until it matches.
- **No sound:** check the buzzer is a *passive* one and the transistor wiring. Try a different frequency.
- **OLED blank:** run 02 first. If it finds 0x3C but shows nothing, check the OLED's address and pin order.
- **Real-time timer:** in 09, change `SECONDS_PER_MINUTE` from `3` to `60`.
- **Turn off flip-to-start:** in 09, set `FLIP_TO_FOCUS` to `0`.

## 6. Ideas for later

- Wi-Fi: sync the time from the internet, or show a clock when idle.
- Bluetooth: control the timer from a phone.
- Deep sleep: use the MPU motion interrupt (D3) to wake the board and save the battery.
- Save the number of finished sessions across restarts.
