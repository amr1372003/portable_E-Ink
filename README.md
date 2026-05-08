# E-Ink Task Manager

A low-power student organizer built on an **ATmega32** microcontroller and a **2.13" e-paper display**, synchronized wirelessly via Bluetooth Classic. The user enters tasks, an upcoming exam, and a daily habit through a React Native Android app, which transmits the data over HC-06 to the microcontroller. The ATmega32 renders the content onto the e-paper display and exposes two hardware buttons for navigation and habit tracking.

> **Course:** Microprocessor Based Systems  
> **Supervised by:** Dr. Medhat Hussien · Eng. Shady Medhat  
> **Team:** Amr Hassan · Batool Sherif · Moataz Hazem

---

## Table of Contents

- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
  - [Pin Table](#pin-table)
  - [Wiring](#wiring)
- [Firmware](#firmware)
  - [Clock Source](#clock-source)
  - [Display Driver](#display-driver)
  - [UART and CSV Parsing](#uart-and-csv-parsing)
  - [Button Handling](#button-handling)
  - [Battery Monitoring](#battery-monitoring)
  - [Timer](#timer)
- [Mobile App](#mobile-app)
  - [Bluetooth Connectivity](#bluetooth-connectivity)
  - [Payload Format](#payload-format)
- [Repository Structure](#repository-structure)
- [Building and Flashing](#building-and-flashing)
- [Further Improvements](#further-improvements)

---

## Features

- Displays up to **10 tasks** with times across paginated pages (4 tasks per page)
- Shows an **upcoming exam** name and date
- **Habit tracker** with filled/unfilled dot progress indicators
- **Battery level** indicator using AVR internal bandgap ADC
- Wireless data entry via **Bluetooth Classic (HC-06 / SPP)**
- Custom **column-scan renderer** — no framebuffer, only 16 bytes of SRAM used for rendering
- **5×7 bitmap font** stored in flash (PROGMEM) to conserve SRAM
- Hardware button **debouncing** and **long-press detection** without blocking delays
- Runs entirely from the **internal 1 MHz RC oscillator** — no external crystal needed

---

## System Architecture

```
┌─────────────────────┐        CSV over       ┌──────────┐       UART 9600      ┌──────────────────────┐
│  React Native App   │ ──── RFCOMM / BT ───▶ │  HC-06   │ ──── baud, \n ─────▶ │     ATmega32         │
│  Android (TypeScript│                        │ BT bridge│                       │  1 MHz internal RC   │
└─────────────────────┘                        └──────────┘                       └──────────┬───────────┘
                                                                                             │
                                                                             ┌───────────────┼───────────────┐
                                                                             │               │               │
                                                                      SPI bus          INT0 / INT1        ADC
                                                                             │               │               │
                                                                    ┌────────┴──────┐ ┌──────┴──────┐  Battery
                                                                    │ E-paper 2.13" │ │  2 buttons  │  voltage
                                                                    │  250×122 px   │ │  Down / Up  │
                                                                    └───────────────┘ └─────────────┘
```

---

## Hardware

### Components

| Component | Part |
|---|---|
| Microcontroller | ATmega32A (DIP-40) |
| Display | 2.13" monochrome e-paper, 250×122 px (GDEH0213B72 compatible) |
| Bluetooth module | HC-06 (Bluetooth Classic, SPP profile) |
| Buttons | 2× tactile push buttons |
| Supply | 3 V (e.g. 2× AA / coin cell) |

### Pin Table

| Signal | ATmega32 Pin | Function |
|---|---|---|
| SDA (display) | PB5 / MOSI | SPI data out |
| SCL (display) | PB7 / SCK | SPI clock |
| CS (display) | PB4 | Chip select (active low) |
| DC (display) | PD6 | Data / command select |
| RES (display) | PD7 | Reset (active low) |
| BUSY (display) | PD5 | Busy status input |
| TX (HC-06) | PD0 / RXD | UART receive from HC-06 |
| Button 0 | PD2 / INT0 | Navigate down / long-press: habit++ |
| Button 1 | PD3 / INT1 | Navigate up |

### Wiring

- All SPI lines (MOSI, SCK, CS, DC, RST) connect from ATmega32 Port B / Port D to the e-paper module.
- HC-06 **TX → PD0 (RXD)**. HC-06 RX is unused (ATmega32 never transmits back).
- Both buttons connect between their signal pin and GND. Internal pull-ups are enabled in firmware.
- The e-paper module operates at **3.3 V logic**. If the ATmega32 is powered at 5 V, add a level shifter on the SPI lines.

---

## Firmware

Source: [`firmware/main.c`](firmware/main.c)

Toolchain: `avr-gcc` · Programmer: `avrdude` (USBasp or compatible)

### Clock Source

The ATmega32 uses its **internal 1 MHz RC oscillator** (factory default fuse). No external crystal is needed. `F_CPU` is defined as `1000000UL`, which correctly calibrates `_delay_ms()`, Timer0 CTC, and the UART baud rate register.

```c
#define F_CPU 1000000UL
```

At 1 MHz and 9600 baud: `UBRR = (1,000,000 / (16 × 9600)) − 1 = 5`

> **Note:** The internal RC oscillator has a ±10% tolerance over temperature and voltage. For stable UART in harsh environments, calibrate `OSCCAL` at startup or use an external crystal.

### Display Driver

The renderer uses a **streamed column-scan architecture** to avoid a full framebuffer:

- A full 250×122 bitmap would require ~4 KB — exceeding the ATmega32's 2 KB SRAM.
- Instead, a single **16-byte `row_buf`** is computed per column by `BuildRow(land_x)` and sent immediately over SPI.
- This iterates across all 250 logical columns, keeping SRAM usage constant.

The display is divided into three regions:

| Region | Y range | Content |
|---|---|---|
| Header | 0 – 21 px | Date (x=4), day name (x=109), battery icon (x=220) |
| Body | 22 – 90 px | Up to 4 tasks per page with right-aligned times |
| Tail | 91 – 122 px | Exam name + date, habit name + dot indicators |

Text is rendered using a **5×7 monochrome bitmap font** stored in `PROGMEM`.

### UART and CSV Parsing

- **256-byte circular ring buffer** with `uint8_t` head/tail indices — natural 8-bit overflow gives correct modulo-256 wrap with no masking.
- `USART_RXC_vect` ISR stores each byte; sets `line_ready = 1` on `'\n'`.
- `Receive_Line()` disables the RX interrupt while draining the buffer, then re-enables it.
- `Parse_CSV()` tokenises with `strtok()` into up to **20 fields × 24 chars**.
- `Extract_And_Update()` maps fields to `HeaderData`, `BodyData`, `TailData` and triggers a full-screen refresh.
- A **minimum of 5 fields** is required before any struct is modified, guarding against malformed payloads.

### Button Handling

| Press type | Duration | Action |
|---|---|---|
| Bounce | < 20 ms | Discarded |
| Short press (Down) | 20 ms – 2000 ms | Page down |
| Short press (Up) | 20 ms – 2000 ms | Page up |
| Long press (Down) | ≥ 2000 ms | `habit_filled++` |
| Long press (Up) | ≥ 2000 ms | Reserved |

- `INT0` / `INT1` ISRs trigger on **falling edge** and start the Timer0 counter — no delays inside ISRs.
- Debounce and duration logic runs in `HandleButtons()`, polled from the main loop.

### Battery Monitoring

Uses the AVR internal **bandgap reference** (1.1 V) to back-calculate VCC:

```
AVCC = (1.1 × 1023) / ADC
```

Five readings are averaged with 80 ms spacing. Voltage is mapped to a percentage in 50 mV steps from 2550 mV (0%) to 3000 mV (100%).

### Timer

Timer0 in **CTC mode**, prescaler 64:

```
OCR0 = (F_CPU / (64 × 1000)) − 1 = 14   →   1 ms interrupt period
```

The ISR increments `int0_press_ms` and `int1_press_ms` while their respective press flags are active.

---

## Mobile App

Source: [`app/App.tsx`](app/App.tsx)

Built with **React Native** (TypeScript). Requires Android 6+.

### Bluetooth Connectivity

- Uses `react-native-bluetooth-classic` (SPP / RFCOMM).
- HC-06 must be **paired in Android Settings first** (PIN: `1234` or `0000`).
- `getBondedDevices()` populates a bottom-sheet device picker.
- Android 12+: requests `BLUETOOTH_SCAN` + `BLUETOOTH_CONNECT`.
- Android 6–11: requests `ACCESS_FINE_LOCATION`.

### Payload Format

```
<date>,<day>,<task1>,<time1>,...,<taskN>,<timeN>,<examName>,<examDate>,<habitName>\n
```

Example:
```
8 May,Friday,Microprocessor,1:00 PM,Analog CMOS,2:00 PM,Digital CMOS Exam,20 Mar,Reading\n
```

- Date: `D MMM` (e.g. `8 May`)
- Day: full weekday name (e.g. `Friday`)
- Tasks and times in **interleaved pairs**
- Exam name, exam date, habit name as the **last three fields**
- Maximum **10 tasks**; excess is silently truncated by firmware

---

## Repository Structure

```
.
├── firmware/
│   └── main.c          # ATmega32 firmware (AVR-GCC)
├── app/
│   └── App.tsx         # React Native mobile app
├── docs/
│   └── report.pdf      # Full project report
└── README.md
```

---

## Building and Flashing

### Firmware

```bash
# Compile
avr-gcc -mmcu=atmega32 -DF_CPU=1000000UL -O2 -o main.elf firmware/main.c

# Convert to hex
avr-objcopy -O ihex main.elf main.hex

# Flash (USBasp)
avrdude -c usbasp -p m32 -U flash:w:main.hex
```

> Fuses: leave at factory defaults (internal 1 MHz RC oscillator, `CKSEL = 0001`).

### Mobile App

```bash
cd app
npm install
npx react-native run-android
```

Requires Android Studio, Android SDK, and a physical Android device (Bluetooth emulation is not supported in the emulator).

---

## Further Improvements

The current codebase uses only ~4.5 KB of flash and ~600 B of SRAM, well within the limits of a smaller microcontroller:

| Feature | ATmega32A (current) | ATmega8A (proposed) | Requirement |
|---|---|---|---|
| Flash | 32 KB | 8 KB | ~4.5 KB |
| SRAM | 2 KB | 1 KB | ~600 B |
| EEPROM | 1024 B | 512 B | text storage |
| Ext. interrupts | 3 | 2 | 2 |
| Peripherals | UART, SPI, I2C | UART, SPI, I2C | UART, SPI |

Other planned improvements:
- **Partial display refresh** — currently all four update functions perform a full 250-column refresh; region-specific updates would reduce flicker and refresh time.
- **UART acknowledgement** — firmware has no TX path; app cannot confirm receipt.
- **Persistent habit state** — `habit_filled` resets on power cycle; EEPROM storage would fix this.
- **OSCCAL calibration** — improve internal oscillator accuracy for more reliable UART.
