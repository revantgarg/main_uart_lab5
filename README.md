# TM4C123 Light + Keypad Stopwatch

A TM4C123GH6PM LaunchPad program combining an LED blink-rate/colour
controller with a stopwatch that can be driven either from the EduARM4
keypad or from the UART console.

## What it does

- Blinks the on-board RGB LED at one of 8 speeds and one of 8 colours.
- Shows the LED state on a 4-digit 7-segment display: a fixed `S`
  label, the current rate, the current colour, and `r` (running) or
  `P` (paused).
- SW1 (on-board) cycles the blink rate; SW2 cycles the colour;
  pressing both together pauses or resumes.
- UART0 (115200-8-N-1) mirrors the LED state and can also drive it:
  `RATE`, `COLOUR`, `PAUSE`, `RESUME`, `STATUS`.
- A stopwatch, controllable from either the keypad or the UART console:

  | Action | Keypad key | UART command |
  |--------|-----------|--------------|
  | Enable / disable | "1" | `SWENABLE` |
  | Start / stop | "2" | `SWSTART` |
  | Pause / resume | "3" | `SWPAUSE` |

- The moment the stopwatch is enabled, the 4-digit display switches
  from the LED readout to the stopwatch's elapsed time (`MM:SS`), and
  switches back when it's disabled.
- `STATUS` over UART reports both the LED state and the stopwatch's
  state and elapsed time.

## Hardware

- TM4C123GH6PM LaunchPad + EduARM4 add-on board
- 7-segment display: Port A (digit select, PA4-PA7), Port B (segments)
- On-board SW1 (PF4), SW2 (PF0), RGB LED (PF1-PF3)
- Keypad: Row 0 → PE0 (driven low, open-drain); Columns 0-2 → PC4/PC5/PC6
  (keys "1"/"2"/"3"), wired as genuine falling-edge GPIO interrupts
- **Jumpers J7-J10 on the EduARM4 board must be open** for the keypad
  to work as plain GPIO

## Timing

A SysTick interrupt fires every 10 ms and serves two purposes: it
advances the stopwatch's elapsed time while running, and it provides
the timestamp used to debounce the three keypad keys (a 200 ms window).

## Interrupt vector table

The project's `tm4c123gh6pm_startup_ccs_gcc.c` routes every interrupt
to a do-nothing default handler and isn't meant to be edited. Instead,
`main.c` relocates the entire vector table into RAM at boot and patches
just two entries — SysTick and GPIO Port C — to point at this
program's real handlers. This is done entirely from `main.c`; no
startup-file changes are needed.

## How the keypad and the UART stay in sync

Three functions hold the actual stopwatch logic: `stopwatchEnableToggle()`,
`stopwatchStartStopToggle()`, and `stopwatchPauseResumeToggle()`. Both
the keypad's interrupt handler and the UART command parser call these
same three functions, so pressing a key and typing its matching
command are guaranteed to do exactly the same thing.

## Building

Standard Code Composer Studio project, GNU ARM compiler. Self-contained
— no TivaWare/driverlib library required. Make sure only one `.c` file
with a `main()` exists in the project; CCS compiles every `.c` file it
finds, so a leftover file from earlier work will cause "multiple
definition" linker errors.

## Using the UART interface

Connect at **115200 baud, 8 data bits, no parity, 1 stop bit**, then
type a command and press Enter (case-sensitive, all caps):

| Command    | Effect                                      |
|------------|------------------------------------------------|
| `RATE`     | Advances the blink rate by one (wraps 7→0)   |
| `COLOUR`   | Advances the colour by one (wraps 7→0)       |
| `PAUSE`    | Pauses the lights (message if already paused) |
| `RESUME`   | Resumes the lights (message if already running) |
| `STATUS`   | Reports the current rate, colour, state, and stopwatch status |
| `SWENABLE` | Enables/disables the stopwatch (same as key "1") |
| `SWSTART`  | Starts/stops the stopwatch (same as key "2") |
| `SWPAUSE`  | Pauses/resumes the stopwatch (same as key "3") |

If a command says "Unknown command" and the spelling/capitalization is
correct, the board is very likely running an older build — do a full
Clean + rebuild in CCS and reflash.

## Notes

- The `STATUS` reply prints the literal word `status` with no line
  break before it, so it runs together with the line that follows.
- The keypad debounce window is 200 ms (`DEBOUNCE_TICKS = 20` at a
  10 ms SysTick period).
