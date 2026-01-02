# CV Pico Sequencer

A versatile CV/Gate sequencer built for the Raspberry Pi Pico. This project implements a 16-step sequencer with adjustable BPM, note editing, and pattern management, featuring an OLED display implementation for visual feedback.

## Features

- **16-Step Sequencer:** Configurable step count (1-16) per pattern.
- **Adjustable BPM:** Tempo range from 20 to 300 BPM.
- **CV/Gate Output:** 
  - 1V/Octave CV output (0-4095 range via DAC).
  - Gate output for envelope triggering.
- **Pattern Management:**
  - 10 Pattern slots (0-9).
  - Save and Load patterns to EEPROM/Flash.
  - Pattern queuing for seamless transitions during playback.
- **Edit Modes:**
  - **Step Select:** Navigate through steps to edit.
  - **Note Edit:** adjust MIDI note (36-84) for each step.
  - **Pattern Select:** Switch between patterns on the fly.
- **Visual Interface:** Designed for SSD1306 OLED display.
- **Hardware Controls:** Rotary encoder and dedicated function buttons.

## Hardware Connections

### Pinout

| functionality         | GPIO Pin | Type   |
|-----------------------|----------|--------|
| **Play/Pause Button** | GP2      | Input  |
| **Stop Button**       | GP7      | Input  |
| **Step Count Button** | GP8      | Input  |
| **Edit Mode Button**  | GP10     | Input  |
| **Pattern Select**    | GP11     | Input  |
| **Save Button**       | GP12     | Input  |
| **Encoder SW**        | GP13     | Input  |
| **Encoder CLK**       | GP14     | Input  |
| **Encoder DT**        | GP15     | Input  |
| **Status LED**        | GP3      | Output |

### Outputs
- **CV Output:** Driven via DAC (implementation dependent, see `clock.cpp`).
- **Gate Output:** Controlled by `clock_gate_enable` (see `clock.cpp`).

## Usage

### Controls
- **Play/Pause:** Toggles sequencer playback and gate output.
- **Stop:** Stops playback and resets to the first step.
- **Encoder:**
  - Rotate to adjust values (BPM, Note, Step Index).
  - Press to toggle sub-modes or confirm actions.
- **Edit Mode:** Cycles through editing steps and notes.
- **Pattern Select:** Enter pattern selection mode. Rotate encoder to choose a slot, press Encoder to load (or queue if playing).

### Managing Patterns
- **Save Pattern:** In Pattern Select or Edit modes, press the **Save Button (GP12)** to save the current pattern to flash memory.
- **Load Pattern:** In Pattern Select mode, select a slot and press the Encoder button.

## Build Instructions

Requirements:
- CMake
- Raspberry Pi Pico SDK

```bash
mkdir build
cd build
cmake ..
make
```

Flash the generated `.uf2` file to your Raspberry Pi Pico.

## Credits

- **Lead Designer:** User
- **Coder:** Antigravity
