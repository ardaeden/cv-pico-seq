# CV Pico Sequencer

A professional-grade, open-source CV/Gate sequencer designed for the Raspberry Pi Pico (RP2040). This project delivers a robust 16-step sequencer with real-time performance features, precise timing, and a rich OLED interface, making it an ideal core for modular synthesizer setups.

## 🚀 Overview

The **CV Pico Sequencer** transforms a standard Raspberry Pi Pico into a capable Eurorack-style sequencer. It features adjustable BPM, variable pattern lengths (1-16 steps), per-step note editing, and a dedicated pattern management system. Built with C++17 and the Pico SDK, it leverages the RP2040's dual-core architecture for rock-solid timing stability.

## ✨ Features

### Core Sequencing
- **16-Step Sequencer:** Fully configurable step count per pattern (1 to 16 steps).
- **Precision Timing:** BPM range from 20 to 300, driven by a dedicated hardware timer on Core 1 for jitter-free clocking.
- **CV/Gate Output:**
  - **CV:** 1V/Octave standard (0-4095 DAC resolution), covering a wide musical range (MIDI notes 36-84).
  - **Gate:** 3.3V digital output for triggering envelopes (requires buffering for Eurorack levels).

### Pattern Management
- **10 Pattern Slots:** Store and recall up to 10 unique patterns (Slots 0-9).
- **Non-Volatile Storage:** Patterns are saved to the onboard flash memory/EEPROM, persisting across power cycles.
- **Performance Queuing:** Switch patterns seamlessly during playback. The next pattern queues and launches perfectly in sync at the end of the current cycle.

### User Interface
- **OLED Display:** Optimized for 128x64 SSD1306 displays, providing clear feedback on BPM, active steps, notes, and editing modes.
- **Intuitive Navigation:** Rotary encoder and dedicated function buttons allow for fast, hands-on control without menu diving.
- **Visual Feedback:** Real-time playback visualization, LED beat indicators, and blinking confirmation for save operations.

---

## 🛠 Hardware Architecture

The system is built around the **Raspberry Pi Pico**.

### Components
- **Microcontroller:** Raspberry Pi Pico (RP2040)
- **Display:** SSD1306 I2C OLED (128x64)
- **DAC:** MCP4822 (12-bit, dual channel) via SPI
- **Input:** Rotary Encoder (with push button) + 6 Tactile Buttons

### Pin Configuration

| Component         | GPIO Pin | Type   | Description |
|-------------------|----------|--------|-------------|
| **Play/Pause**    | GP2      | Input  | Toggles playback / Gate enable |
| **Stop**          | GP7      | Input  | Stops playback, resets to Step 1 |
| **Step Count**    | GP8      | Input  | Hold + Encoder to set pattern length |
| **Edit Mode**     | GP10     | Input  | Cycles: View -> Edit Steps -> Edit Notes |
| **Pattern Select**| GP11     | Input  | Enter Pattern Select / Queue Mode |
| **Save**          | GP12     | Input  | Save current pattern to slot |
| **Encoder SW**    | GP13     | Input  | Confirm / Toggle Sub-modes |
| **Encoder CLK**   | GP14     | Input  | Rotary Encoder Clock |
| **Encoder DT**    | GP15     | Input  | Rotary Encoder Data |
| **Status LED**    | GP3      | Output | Visual Beat Indicator |

> **Note:** For Eurorack integration, ensure proper voltage scaling/buffering. The Pico outputs 0-3.3V, while Eurorack typically expects 0-5V or 0-10V for CV and Gates.

---

## 💻 Software Architecture

The codebase is modular, written in **C++17**, and structured for maintainability and performance.

### Directory Structure
- **`main.cpp`**: Application entry point, main run loop, and event orchestration.
- **`sequencer.cpp / .h`**: Core logic for step handling, pattern data, and playback state.
- **`clock.cpp / .h`**: High-precision timing engine. managing BPM, ticks, and DAC/Gate hardware updates.
- **`ui.cpp / .h`**: Drawing routines for the SSD1306 OLED, managing menus and visual state.
- **`io.cpp / .h`**: Hardware abstraction layer for GPIO, Button debouncing, and Encoder interrupts.
- **`eeprom.cpp / .h`**: Storage abstraction for saving/loading patterns to flash memory.

### Key Libraries
- **Pico SDK:** `pico_stdlib`, `pico_multicore`, `hardware_timer`, `hardware_i2c`, `hardware_spi`.

---

## 🚀 Getting Started

### Prerequisites
1.  **CMake** (3.13 or later)
2.  **ARM GCC Toolchain** (`arm-none-eabi-gcc`)
3.  **Raspberry Pi Pico SDK** installed and configured in your environment.

### Build Instructions

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/your-username/cv-pico-seq.git
    cd cv-pico-seq
    ```

2.  **Create a build directory:**
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure CMake:**
    ```bash
    cmake ..
    ```

4.  **Compile:**
    ```bash
    make
    ```

5.  **Flash:**
    - Hold the **BOOTSEL** button on your Pico and plug it via USB.
    - Copy the generated `cv-pico-seq.uf2` file to the mounted RPI-RP2 drive.
    - OR use picotool:
      ```bash
      sudo picotool load -v -x cv-pico-seq.uf2 -f
      ```

---

## 📖 User Manual

### Main Screen
Displays the current **BPM** (Tempo) and the **Step Grid**. The active step is highlighted during playback.
- **Rotate Encoder:** Adjust BPM.
- **Hold "Step Count" (GP8) + Rotate Encoder:** Change pattern length (1-16 steps).

### Playback Control
- **Play/Pause (GP2):** Starts or pauses the sequence.
- **Stop (GP7):** Stops the sequence and resets the playhead to the first step.

### Editing Patterns
1.  **Step Enable/Disable (Gate):**
    - Press **Edit Mode (GP10)** once to enter "Edit Step" mode.
    - Rotate Encoder to select a step.
    - Press **Save (GP12)** to toggle the Gate for that step.
    
2.  **Note Editing (Pitch):**
    - Press **Edit Mode** again to enter "Edit Note" mode.
    - Rotate Encoder to change the pitch (Note) for the selected step.
    - Press **Save (GP12)** to toggle Gate (optional convenience).

### Pattern Management
1.  **Select/Load Pattern:**
    - Press **Pattern Select (GP11)**.
    - Rotate Encoder to choose a slot (0-9).
    - **Press Encoder Button** to Load.
      - *If playing:* The pattern is **Queued** and will load automatically when the current pattern finishes.
      - *If stopped:* Loads immediately.

2.  **Save Pattern:**
    - In "Pattern Select" mode, choose your target slot.
    - Press **Save (GP12)**.
    - The screen will blink to confirm the save.

---

## 🏆 Credits

- **Lead Designer:** User
- **Coder:** Antigravity
