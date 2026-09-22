# TM4C123 LED Blinky — Stopwatch Extension

Extends the base rate/colour LED blinky assignment with a SysTick-driven
stopwatch, controllable from the serial console or a 4×4 keypad, per the
assignment brief.

## Files

| File | Purpose |
|---|---|
| `main.c` | Application: LED blink/colour logic, UART command parser, stopwatch state machine, keypad + switch interrupt handlers, 7-segment display dispatch. |
| `startup_ccs_patched.c` | Your project's startup file, with `SysTick_Handler` and `GPIOPortC_Handler` wired into the vector table (entries 15 and 18). Drop this in place of your existing startup file. |

## Hardware wiring (EduARM4 board)

- **7-segment module:** digit select on PA4–PA7, segment pattern on PB0–PB7 (unchanged from the base assignment).
- **Onboard RGB LED:** PF1–PF3 (unchanged).
- **Onboard switches SW1 / SW2:** PF4 / PF0, active low, pull-ups enabled — now handled via a GPIO Port F edge interrupt instead of polling.
- **Keypad (stopwatch control only):**
  - Row 0 → PE0, driven permanently low (open-drain).
  - Column 0 → PC4 → key **`1`** → Enable / Disable stopwatch.
  - Column 1 → PC5 → key **`2`** → Start / Stop.
  - Column 2 → PC6 → key **`3`** → Pause / Resume.
  - A falling edge on PC4–PC6 fires `GPIOPortC_Handler` directly — no scanning needed, since only one row is ever active.
- **UART0:** PA0/PA1, 115200 baud, 8N1 (unchanged).

## Serial console commands

| Command | Effect |
|---|---|
| `RATE` | Cycle LED blink rate (0–7). |
| `COLOUR` | Cycle LED colour (0–7). |
| `PAUSE` / `RESUME` | Pause/resume the LED blink. |
| `STATUS` | Print current rate/colour/run state. |
| `SWENABLE` | Toggle whether the 7-segment display shows the stopwatch (MM:SS) or the rate/colour/state readout. |
| `SWSTART` | Start the stopwatch from 0, or stop and reset it to 0 if already running/paused. |
| `SWPAUSE` | Pause the stopwatch if running, or resume it if paused. |
| `SWSTATUS` | Print current stopwatch state and elapsed MM:SS. |

Commands are case-sensitive and terminated with `\r` or `\n`.

## Timing

A SysTick interrupt fires every 10 ms (`msTicks`), which:
- increments the stopwatch's elapsed-time counter (`swCentis`, in hundredths of a second) whenever it's running, and
- provides the timestamp base for debouncing both the keypad and SW1/SW2 (20-tick / 200 ms minimum gap between accepted presses).

## Building

1. Replace your project's existing startup file with `startup_ccs_patched.c` (or apply the same two-line vector table change to your own copy — see the report for exactly which lines changed).
2. Replace/add `main.c`.
3. Rebuild. No other project settings need to change — clock source, pin muxing for UART0/7-seg, and all peripheral clocks are configured in code as before, with Port C and Port E clocks added for the keypad.
scale accordingly to keep the 10 ms tick accurate.
- The stopwatch display currently shows `MM:SS` (minutes:seconds, two digits each) rather than the assignment's literal `S:S:ms:ms` wording — adjust `displayStopwatch()` if your grader expects the field split differently (e.g. seconds:centiseconds instead of minutes:seconds).
