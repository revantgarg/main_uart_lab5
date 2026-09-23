# TM4C123 LED Blinky — Stopwatch Extension

Extends the base rate/colour LED blinky assignment with a SysTick-driven
stopwatch, controllable from the serial console or a 4×4 keypad.

## Files

| File | Purpose |
|---|---|
| `main.c` | Application: LED blink/colour logic, UART command parser, stopwatch state machine, keypad + switch interrupt handlers, 7-segment display dispatch. |
| `startup_ccs_patched.c` | project's startup file, with `SysTick_Handler` and `GPIOPortC_Handler` wired into the vector table (entries 15 and 18).

## Hardware wiring (EduARM4 board)

- **7-segment module:** digit select on PA4–PA7, segment pattern on PB0–PB7 (unchanged from the base assignment).
- **Onboard RGB LED:** PF1–PF3 (unchanged).
- **Onboard switches SW1 / SW2:** PF4 / PF0, active low, pull-ups enabled — now handled via a GPIO Port F edge interrupt instead of polling.
- **Keypad (stopwatch control only):**
  - Row 0 → PE0, driven permanently low (open-drain).
  - Column 0 → PC4 → key **`1`** → Enable / Disable stopwatch (alternates based on current state).
  - Column 1 → PC5 → key **`2`** → Start / Stop (alternates based on current state).
  - Column 2 → PC6 → key **`3`** → Pause / Resume (alternates based on current state).
  - A falling edge on PC4–PC6 fires `GPIOPortC_Handler` directly — no scanning needed, since only one row is ever active. The handler picks the right explicit action (see below) for the current state, so each key still does one of two things depending on context, per the assignment brief.
- **UART0:** PA0/PA1, 115200 baud, 8N1 (unchanged).

## Serial console commands

| Command | Effect |
|---|---|
| `RATE` | Cycle LED blink rate (0–7). |
| `COLOUR` | Cycle LED colour (0–7). |
| `PAUSE` / `RESUME` | Pause/resume the LED blink. |
| `STATUS` | Print current rate/colour/run state. |
| `SWENABLE` | Enable the stopwatch display (7-seg switches from status to MM:SS). No-op with a message if already enabled. |
| `SWDISABLE` | Disable the stopwatch display, reverting the 7-seg to the rate/colour/state readout. No-op with a message if already disabled. |
| `SWSTART` | Start the stopwatch from 0. Only valid when enabled and idle. |
| `SWSTOP` | Stop the stopwatch and reset it to 0. Valid when running or paused. |
| `SWPAUSE` | Pause the stopwatch. Only valid when running. |
| `SWRESUME` | Resume the stopwatch. Only valid when paused. |
| `SWSTATUS` | Print current stopwatch state and elapsed MM:SS. |

Each stopwatch command is a single explicit action (mirroring `RATE`/`COLOUR`/`PAUSE`/`RESUME` on the LED side) rather than a toggle — calling one from the wrong state prints an "already X" / "not valid" message instead of silently flipping state. Commands are case-sensitive and terminated with `\r` or `\n`.

## Timing

A SysTick interrupt fires every 10 ms (`msTicks`), which:
- increments the stopwatch's elapsed-time counter (`swCentis`, in hundredths of a second) whenever it's running, and
- provides the timestamp base for debouncing both the keypad and SW1/SW2 (20-tick / 200 ms minimum gap between accepted presses).
