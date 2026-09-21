# Stopwatch  Control
## What it does

- Everything from Lab 4 (LED blink rate/colour, controllable by SW1/SW2
  or by `RATE`/`COLOUR`/`PAUSE`/`RESUME`/`STATUS` over UART).
- A stopwatch, added on top, driven entirely by three keys on the
  EduARM4 keypad:

  | Key | Action |
  |-----|--------|
  | "1" | Enable / disable the stopwatch |
  | "2" | Start / stop the stopwatch |
  | "3" | Pause / resume the stopwatch |

- The keys are wired as real edge-triggered GPIO interrupts (Row 0 on
  PE0 held low, columns 0-2 on PC4/PC5/PC6), not polled scanning.
- The 4-digit display automatically switches from the LED readout to
  the stopwatch's `MM:SS` the moment the stopwatch is enabled.
- `STATUS` over UART still reports the stopwatch's state and time —
  you can *watch* it from the console, you just can't *control* it
  from there.

## Hardware

- TM4C123GH6PM LaunchPad + EduARM4 board
- Keypad Row 0 → PE0 (driven low, open-drain); Columns 0-2 → PC4/PC5/PC6
- **Jumpers J7-J10 on the EduARM4 board must be open**

## Building

Standard CCS project, GNU ARM compiler, no external library. Make sure
this is the only `.c` file with a `main()` in the project.
