<!--
DRAFT BUILD NOTES. Written by Claude from my screenshots and chat on 2026-11-08.
Use these to write my own journal and README in my own words.
Delete this comment once I've rewritten it.
-->

# XIAO ESP32-C3 Pomodoro Timer: build notes

A small battery-powered Pomodoro timer on a custom PCB. A Seeed XIAO ESP32-C3 does the Wi-Fi/BLE and logic, a small I2C OLED shows the timer, and a 2S Li-ion pack (7.4 V) is stepped down to 5 V by an AP63205 buck converter.

I took the idea from the "Pomodoro Bot" title only. The hardware is my own design, not a copy of that project.

Status: schematic in progress (KiCad). Power and display sections drawn, ERC not run yet.

## Plan

| Block | Choice |
|---|---|
| MCU | XIAO ESP32-C3 (SMD), Wi-Fi and BLE built in |
| Power in | 2S Li-ion/LiPo pack, 7.4 V nominal, 8.4 V full |
| Regulator | AP63205 buck, 5 V fixed output, 3.8 V to 32 V in, 2 A |
| 5 V into the XIAO | Through a Schottky diode into the 5V (VBUS) pin; the XIAO makes its own 3.3 V |
| Display | 0.96" 128x64 I2C OLED module on a 4-pin header |
| Planned, not drawn yet | Slide switch, reverse-polarity protection, battery-level divider, buttons, buzzer, status LED |

## Session 1: 2026-11-08

Time spent: _(fill in)_

### 1. Display header

I needed somewhere to plug in the OLED. KiCad has two OLED symbols, but I wasn't sure they were for a ready-made module, so I used a plain 4-pin header (`oledDisplay`, J1) instead. A module only needs four wires.

![Display header symbol](images/journal-01/01_display-header-symbol.png)

### 2. XIAO symbol

I placed the XIAO-ESP32-C3-SMD symbol. Things I noticed on it:
- `VBUS` (pin 14) is the 5 V pin and `3V3_OUT` (pin 12) is the output of the XIAO's own regulator.
- It has a `VBAT` pin (pin 21), which is the XIAO's single-cell battery input. My pack is 7.4 V, so **VBAT stays unconnected.**
- `D4` and `D5` are the pins I plan to use for I2C (SDA and SCL). I still need to check this against Seeed's pinout for the C3.

![XIAO ESP32-C3 symbol](images/journal-01/02_xiao-esp32-c3-symbol.png)

### 3. Display header wired

| Header pin | Net |
|---|---|
| 1 | GND |
| 2 | +3V3 |
| 3 | SCL |
| 4 | SDA |

Open check: pin order differs between OLED modules (some are VCC first). I need to match this to the module I actually buy before ordering the PCB. SCL and SDA are only labels so far; they still need to be joined to the XIAO pins.

![Display header wired](images/journal-01/03_display-header-wired.png)

### 4. Buck converter: choosing the part and the 5 V decision

I wanted to run from a 7.4 V pack. I checked Seeed's getting-started page for the C3: the 5V pin can be used as an input if there is a diode in series (anode to the supply, cathode to the 5V pin). So the plan is a 5 V buck feeding that pin through a Schottky, and the XIAO's regulator makes 3.3 V for the display.

Two other points from that research:
- The XIAO's built-in charger only handles a single cell, so a 2S pack has to be charged off the board with a BMS/charger module.
- The same page gives two different limits for the 3V3 pin (700 mA and 500 mA), so I'll treat it as 500 mA to be safe.

For the regulator I used the AP63205 datasheet's typical application (Figure 21): 4.7 µH inductor, 10 µF input cap, 2 x 22 µF output caps, 100 nF bootstrap cap. IN and EN are tied together. There are no feedback resistors because the 5 V version has a fixed output, and FB goes straight to the output.

![AP63205 symbol](images/journal-01/04_buck-ap63205-symbol.png)

### 5. First attempt, and what was wrong

![Buck first attempt](images/journal-01/05_buck-first-attempt.png)

Problems:
1. **Input cap in series.** C2 (22 nF) sat between the +5V label and the IN pin. A capacitor blocks DC, so the chip would never get power. The input capacitor belongs from IN to GND, and I changed it to 10 µF.
2. **BST had a no-connect flag.** The bootstrap pin needs 100 nF to SW. The 100 nF I had was on the output instead.
3. **Output side had no ground.** The output caps and divider hung between the inductor output and a net that went nowhere.
4. **A feedback divider on a fixed part.** The 10k/10k divider is for the adjustable AP63200. On the fixed AP63205, FB goes to the output.
5. **Wrong net names.** I had `+5V` as the input and `+3V3` as the output. The part makes 5 V, and the input is the battery, not 5 V.

### 6. Second attempt

I redrew it from the datasheet figure and renamed the nets `VBAT_SW` (input) and `+5V` (output). Two mistakes were left in it:
1. **C1 and C2 were in series** instead of in parallel. In the datasheet "2x22 µF" means two caps side by side. Wired in series they would give about 11 µF.
2. **The inductor value read 4.1 µH.** It should be 4.7 µH.

![Buck second attempt](images/journal-01/06_buck-second-attempt.png)

### 7. Final buck (corrected)

After the fixes C1 and C2 are in parallel between +5V and GND, and L1 reads 4.7 µH. I also added `PWR_FLAG` symbols on `VBAT_SW` and `+5V` so ERC doesn't complain about undriven power pins. Still to add: a `PWR_FLAG` on GND.

_(Screenshot to add: `07_buck-final.png`, once saved.)_

### Things I learned
- A series capacitor blocks DC. Input and output capacitors go from the rail to ground.
- Fixed-output and adjustable regulators are wired differently at the FB pin.
- "2x22 µF" means two in parallel.
- `PWR_FLAG` is needed in KiCad so ERC knows a net is powered.
- Check datasheet figures against the part I'm actually using; the same figure covers several parts (AP63203 and AP63205).

## To do next
- [ ] Battery connector, slide switch, and reverse-polarity protection (P-channel MOSFET)
- [ ] Battery-level divider (100k / 47k) into an ADC pin
- [ ] Schottky from `+5V` to the XIAO 5V pin
- [ ] Connect SDA and SCL to the XIAO; set the display header pin order for the module I buy
- [ ] Buttons, buzzer, status LED
- [ ] Add `PWR_FLAG` on GND, run ERC, save a screenshot of 0 errors
- [ ] Choose real parts: caps rated 16 V or more on the input and 10 V or more on the output, and a shielded 4.7 µH inductor
- [ ] Footprints, then PCB layout

## Sources
- [Seeed XIAO ESP32-C3 getting started](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
- [AP63200 / AP63201 / AP63203 / AP63205 datasheet](https://www.mouser.com/datasheet/2/115/AP63200_AP63201_AP63203_AP63205-1525010.pdf)
