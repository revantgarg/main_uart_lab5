#include <stdint.h>
#include <string.h>
#include "inc/tm4c123gh6pm.h"

#define RUNNING_CHAR 0x50
#define PAUSED_CHAR  0x73
#define S_CHAR       0x6D

#define WAIT_COUNT 50
#define DEBOUNCE_TICKS 20
int rate = 0;
int colour = 0;
int isLightsPaused = 0;

static char uartBuffer[16];
static uint8_t uartIndex = 0;

typedef enum
{
    SW_DISABLED = 0,   // stopwatch off; display shows the lab4 LED readout
    SW_IDLE,           // enabled, elapsed = 0, not counting
    SW_RUNNING,        // counting up
    SW_PAUSED          // counting frozen, elapsed retained
} StopwatchState;

volatile StopwatchState swState = SW_DISABLED;
volatile uint32_t swCentis = 0;      // elapsed time, hundredths of a second
volatile uint32_t msTicks = 0;       // free-running SysTick counter (10 ms/tick)
static volatile uint32_t lastPress[3] = {0, 0, 0};   // debounce timestamps for keys 1,2,3


uint8_t getHEXNumber(int number)
{
    switch(number)
    {
    case 0:
        return 0x3F;

    case 1:
        return 0x06;

    case 2:
        return 0x5B;

    case 3:
        return 0x4F;

    case 4:
        return 0x66;

    case 5:
        return 0x6D;

    case 6:
        return 0x7D;

    case 7:
        return 0x07;

    case 8:
        return 0x7F;

    case 9:
        return 0x6F;

    default:
        return 0x00;
    }
}



void changeColour(int colourValue){
    switch(colourValue)
            {
            case 0:
                GPIO_PORTF_DATA_R = 0x02;
                break;

            case 1:
                GPIO_PORTF_DATA_R = 0x08;
                break;

            case 2:
                GPIO_PORTF_DATA_R = 0x04;
                break;

            case 3:
                GPIO_PORTF_DATA_R = 0x0E;
                break;

            case 4:
                GPIO_PORTF_DATA_R = 0x02 | 0x04;
                break;

            case 5:
                GPIO_PORTF_DATA_R = 0x02 | 0x08;
                break;

            case 6:
                GPIO_PORTF_DATA_R = 0x04 | 0x08;
                break;

            case 7:
                GPIO_PORTF_DATA_R = 0;
                break;

            default:
                GPIO_PORTF_DATA_R = 0x0E;
                break;
            }

}

void displayDigit(int digit, uint8_t pattern)
{
    GPIO_PORTA_DATA_R &= ~0xF0;

    GPIO_PORTB_DATA_R = pattern;

    if(digit == 1)
    {
        GPIO_PORTA_DATA_R |= 0x10;
    }
    else if(digit == 2)
    {
        GPIO_PORTA_DATA_R |= 0x20;
    }
    else if(digit == 3)
    {
        GPIO_PORTA_DATA_R |= 0x40;
    }
    else if(digit == 4)
    {
        GPIO_PORTA_DATA_R |= 0x80;
    }
}


void delayRefresh(int delay)
{
    for(int j = 0; j < delay; j++)
    {
    }
}


void refresh7SegmentDisplay(int isLightsPausedValue, int colourValue, int rateValue)
{
    displayDigit(4, S_CHAR);
    delayRefresh(500);

    displayDigit(3, getHEXNumber(rateValue + 1));
    delayRefresh(500);

    displayDigit(2, getHEXNumber(colourValue));
    delayRefresh(500);

    displayDigit(1,
                 isLightsPausedValue == 0 ? RUNNING_CHAR : PAUSED_CHAR);
    delayRefresh(500);
}

void displayStopwatch(uint32_t centis)
{
    uint32_t totalSeconds = centis / 100;
    uint32_t minutes = (totalSeconds / 60) % 100;   // clamp to 2 digits
    uint32_t seconds = totalSeconds % 60;

    displayDigit(4, getHEXNumber(minutes / 10));
    delayRefresh(500);

    displayDigit(3, getHEXNumber(minutes % 10));
    delayRefresh(500);

    displayDigit(2, getHEXNumber(seconds / 10));
    delayRefresh(500);

    displayDigit(1, getHEXNumber(seconds % 10));
    delayRefresh(500);
}
//check whether to show stopwatch or colour sequence
void updateDisplay(void)
{
    if(swState == SW_DISABLED)
    {
        refresh7SegmentDisplay(isLightsPaused, colour, rate);
    }
    else
    {
        displayStopwatch(swCentis);
    }
}

void UART0_Init(void)
{

    SYSCTL_RCGCUART_R |= 0x01; // we will try to enable the clock for uart console

    delayRefresh(10);

    GPIO_PORTA_AFSEL_R |= 0x03;                 // we will try to have alternate function for Port a instead of the regular gpio
    GPIO_PORTA_PCTL_R   = (GPIO_PORTA_PCTL_R & 0xFFFFFF00) |
                           GPIO_PCTL_PA0_U0RX | GPIO_PCTL_PA1_U0TX;
    GPIO_PORTA_DEN_R   |= 0x03;                 // digital enable PA0, PA1
    GPIO_PORTA_AMSEL_R &= ~0x03;                // no analog on PA0, PA1

    UART0_CTL_R &= ~UART_CTL_UARTEN; // we need to disable the uart while we are configuring as these are critical and we might create issue. requires UARTCTL not be modified while the UART is enabled

    // Baud rate = 115200 with a 16 MHz system clock.
    //    BRD = 16,000,000 / (16 * 115200) = 8.6806
    //    UARTFBRD = 0.6806 * 64 + 0.5 = 44
    UART0_IBRD_R = 8; // only the integer part
    UART0_FBRD_R = 44;

    // 8 data bits, 1 stop bit, no parity, FIFOs disabled.
    UART0_LCRH_R = UART_LCRH_WLEN_8;

    // we will use system clock
    UART0_CC_R = 0x0;

    // enableuart tx and rx
    UART0_CTL_R |= (UART_CTL_UARTEN | UART_CTL_TXE | UART_CTL_RXE);
}

// Blocking transmit of a single character.
void UART0_SendChar(char data)
{
    while (UART0_FR_R & UART_FR_TXFF) { }   // wait while Tx is full
    UART0_DR_R = data;
}

// Blocking transmit of a null-terminated string.
void UART0_SendString(const char *str)
{
    while (*str)
    {
        UART0_SendChar(*str);
        str++;
    }
}

// Non-blocking check, mirroring UARTCharsAvail from the TivaWare sample,
// so the calling code can poll it once per main-loop pass without ever
// stalling on an empty RX.
int UART0_CharAvail(void)
{
    return ((UART0_FR_R & UART_FR_RXFE) == 0);   // RXFE==0 means a byte is waiting
}

// Blocking read of a single character.
char UART0_GetChar(void)
{
    while (UART0_FR_R & UART_FR_RXFE) { }   // wait for a byte to arrive
    return (char)(UART0_DR_R & 0xFF);
}


// --- UART: report current LED/blink state ---------------------------------
void sendStatus(void)
{
    UART0_SendString("Rate: ");
    UART0_SendChar((char)('0' + rate));
    UART0_SendString("  Colour: ");
    UART0_SendChar((char)('0' + colour));
    UART0_SendString("  State: ");
    UART0_SendString(isLightsPaused ? "PAUSED" : "RUNNING");
    UART0_SendString("\r\n");
}

// --- UART: report current stopwatch state ---------------------------------
void sendStopwatchStatus(void)
{
    const char *label;
    uint32_t totalSeconds = swCentis / 100;
    uint32_t minutes = (totalSeconds / 60) % 100;
    uint32_t seconds = totalSeconds % 60;

    switch(swState)
    {
    case SW_DISABLED: label = "DISABLED"; break;
    case SW_IDLE:      label = "IDLE";     break;
    case SW_RUNNING:   label = "RUNNING";  break;
    case SW_PAUSED:    label = "PAUSED";   break;
    default:           label = "?";        break;
    }

    UART0_SendString("Stopwatch: ");
    UART0_SendString(label);
    UART0_SendString("  Time: ");
    UART0_SendChar((char)('0' + (minutes / 10)));
    UART0_SendChar((char)('0' + (minutes % 10)));
    UART0_SendChar(':');
    UART0_SendChar((char)('0' + (seconds / 10)));
    UART0_SendChar((char)('0' + (seconds % 10)));
    UART0_SendString("\r\n");
}

void pollUART(void)
{
    if(!UART0_CharAvail())
    {
        return;
    }

    char c = UART0_GetChar();
    UART0_SendChar(c);

    if(c == '\r' || c == '\n')
    {
        uartBuffer[uartIndex] = '\0';

        if(strcmp(uartBuffer, "RATE") == 0)
        {
            rate = (rate + 1) > 7 ? 0 : rate + 1;
            sendStatus();
        }
        else if(strcmp(uartBuffer, "COLOUR") == 0)
        {
            colour = (colour + 1) > 7 ? 0 : colour + 1;
            changeColour(colour);
            sendStatus();
        }
        else if(strcmp(uartBuffer, "PAUSE") == 0)
        {
            if(!isLightsPaused){
            isLightsPaused = !isLightsPaused;
            sendStatus();
            }
            else{
                UART0_SendString("Light is already paused");
            }
        }
        else if(strcmp(uartBuffer, "STATUS") == 0)
        {
            UART0_SendString("status");
            sendStatus();
        }
        else if(strcmp(uartBuffer, "RESUME") == 0)
                {
            if(isLightsPaused){
            isLightsPaused = !isLightsPaused;
                    sendStatus();
            }
            else{
                UART0_SendString("Light is already running");
            }
                }
        else if(uartIndex > 0)
        {
            UART0_SendString("Unknown command\r\n");
        }

        uartIndex = 0;
    }
    else if(uartIndex < (sizeof(uartBuffer) - 1))
    {
        uartBuffer[uartIndex++] = c;
    }
}


// =========================================================================
// Keypad stopwatch (GPIO-interrupt driven)
// =========================================================================
//
// EduARM4 board wiring (from the board's schematic):
//   Row 0    -> PE0   (we drive this permanently low, open-drain)
//   Column 0 -> PC4   (key "1")  -> Enable / Disable
//   Column 1 -> PC5   (key "2")  -> Start / Stop
//   Column 2 -> PC6   (key "3")  -> Pause / Resume
//
// Because all three keys share one row that's held low all the time,
// a falling edge on a column pin is unambiguous: it can only mean the
// key at that column, in that row, was just pressed. No scanning is
// needed, which is what lets this be a genuine interrupt (vs. the
// polled scanning method used for the plain 4x4-keypad exercise).
//
// The project's startup file (tm4c123gh6pm_startup_ccs_gcc.c) points every
// interrupt at IntDefaultHandler (an infinite loop) and isn't ours to edit.
// So instead of touching it, we relocate the vector table into RAM at boot,
// patch just the two entries we need, and tell the CPU to use that copy
// instead - the flash table (and the startup file) is never modified.
//
// Vector index 15 = SysTick, index 18 = IRQ2 = GPIO Port C. These numbers
// come from the standard Cortex-M exception numbering (system exceptions
// are 1-15, external interrupts IRQ0.. start at 16) and match the order
// of g_pfnVectors[] in the startup file.
#define NUM_VECTORS 155
extern void (* const g_pfnVectors[])(void);     // the flash-resident table
static uint32_t ramVectors[NUM_VECTORS] __attribute__((aligned(1024)));

void SysTick_Handler(void);       // defined further down, in the ISR section
void GPIOPortC_Handler(void);     // defined further down, in the ISR section

void RelocateVectorTable(void)
{
    for(int i = 0; i < NUM_VECTORS; i++)
    {
        ramVectors[i] = (uint32_t)g_pfnVectors[i];
    }

    ramVectors[15] = (uint32_t)SysTick_Handler;     // was IntDefaultHandler
    ramVectors[18] = (uint32_t)GPIOPortC_Handler;   // was IntDefaultHandler

    *((volatile uint32_t *)0xE000ED08) = (uint32_t)ramVectors;   // NVIC VTABLE
}

void Stopwatch_Init(void)
{
    // Point the CPU at our patched RAM copy of the vector table before
    // anything below can possibly fire an interrupt.
    RelocateVectorTable();

    // Port E, pin 0: the keypad row we use, held low all the time.
    GPIO_PORTE_DIR_R |= 0x01;      // PE0 output
    GPIO_PORTE_ODR_R |= 0x01;      // open-drain (matches the keypad app note:
                                   // protects the pin if two keys in the same
                                   // column are pressed at once)
    GPIO_PORTE_DEN_R |= 0x01;
    GPIO_PORTE_DATA_R &= ~0x01;    // drive the row low

    // Port C, pins 4-6: the three keypad columns we're reading.
    GPIO_PORTC_AFSEL_R &= ~0x70;   // plain GPIO, not an alternate function
    GPIO_PORTC_AMSEL_R &= ~0x70;   // digital, not analog
    GPIO_PORTC_DIR_R   &= ~0x70;   // inputs
    GPIO_PORTC_PUR_R   |= 0x70;    // pull-ups (reads high until a key pulls it low)
    GPIO_PORTC_DEN_R   |= 0x70;

    // Interrupt setup: edge-sensitive, single edge, falling edge.
    GPIO_PORTC_IS_R  &= ~0x70;     // edge-sensitive (not level)
    GPIO_PORTC_IBE_R &= ~0x70;     // single edge (not both)
    GPIO_PORTC_IEV_R &= ~0x70;     // 0 = falling edge
    GPIO_PORTC_ICR_R  = 0x70;      // clear any stale flags
    GPIO_PORTC_IM_R  |= 0x70;      // unmask PC4, PC5, PC6

    NVIC_EN0_R |= (1 << 2);        // enable IRQ 2 = GPIO Port C

    // SysTick: periodic 10 ms tick off a 16 MHz system clock, used both
    // as the stopwatch's timebase and as the debounce clock.
    NVIC_ST_CTRL_R    = 0;                 // disable while configuring
    NVIC_ST_RELOAD_R  = 160000 - 1;        // 16,000,000 * 0.010 s - 1
    NVIC_ST_CURRENT_R = 0;                 // clear current value
    NVIC_ST_CTRL_R    = 0x07;              // ENABLE | INTEN | use system clock
}

void SysTick_Handler(void)
{
    msTicks++;

    if(swState == SW_RUNNING)
    {
        swCentis++;
        if(swCentis >= 360000)   // wrap after 1 hour (100 * 3600)
        {
            swCentis = 0;
        }
    }
}

void GPIOPortC_Handler(void)
{
    uint32_t status = GPIO_PORTC_MIS_R & 0x70;   // which of PC4/5/6 fired

    if((status & 0x10) && (msTicks - lastPress[0] > DEBOUNCE_TICKS))   // key "1"
    {
        lastPress[0] = msTicks;

        if(swState == SW_DISABLED)
        {
            swState = SW_IDLE;
            swCentis = 0;
        }
        else
        {
            swState = SW_DISABLED;
            swCentis = 0;
        }
        sendStopwatchStatus();
    }

    if((status & 0x20) && (msTicks - lastPress[1] > DEBOUNCE_TICKS))   // key "2"
    {
        lastPress[1] = msTicks;

        if(swState == SW_IDLE)
        {
            swState = SW_RUNNING;
        }
        else if(swState == SW_RUNNING || swState == SW_PAUSED)
        {
            swState = SW_IDLE;
            swCentis = 0;
        }
        sendStopwatchStatus();
    }

    if((status & 0x40) && (msTicks - lastPress[2] > DEBOUNCE_TICKS))   // key "3"
    {
        lastPress[2] = msTicks;

        if(swState == SW_RUNNING)
        {
            swState = SW_PAUSED;
        }
        else if(swState == SW_PAUSED)
        {
            swState = SW_RUNNING;
        }
        sendStopwatchStatus();
    }

    GPIO_PORTC_ICR_R = 0x70;   // clear all three flags, whichever fired
}


int main(void)
{
    int switch1Value = 1;
    int switch2Value = 1;

    int delay;

    int waiting = 0;
    int waitCount = 0;


    // Port A, B, C, E, F clocks: A/B/F for the 7-seg + on-board switches
    // (as before), C for the keypad columns, E for the keypad row.
    SYSCTL_RCGC2_R |= 0x00000037;

    GPIO_PORTF_LOCK_R = 0x4C4F434B;
    GPIO_PORTF_CR_R |= 0x01;

    GPIO_PORTF_DIR_R = 0x0E;
    GPIO_PORTF_PUR_R = 0x11;
    GPIO_PORTF_DEN_R = 0x1F;

    GPIO_PORTA_DIR_R |= 0xF0;
    GPIO_PORTA_DEN_R |= 0xF0;

    GPIO_PORTB_DIR_R = 0xFF;
    GPIO_PORTB_DEN_R = 0xFF;

    UART0_Init();
    Stopwatch_Init();

    UART0_SendString("LED Blinky, RATE FOR INCREMENTING THE RATE, COLOUR FOR INCREMENTING THE COLOUR, PAUSE FOR PAUSING THE LIGHTS , RUNNING FOR RESUMING, STATUS FOR STATUS");
    UART0_SendString("Keypad: 1=Enable/Disable stopwatch  2=Start/Stop  3=Pause/Resume\r\n");
    sendStatus();


    while(1)
    {
        switch(rate)
        {
        case 0:
            delay = 2000;
            break;

        case 1:
            delay = 1500;
            break;

        case 2:
            delay = 1000;
            break;

        case 3:
            delay = 750;
            break;

        case 4:
            delay = 500;
            break;

        case 5:
            delay = 200;
            break;

        case 6:
            delay = 100;
            break;

        case 7:
            delay = 50;
            break;

        default:
            delay = 250;
            break;
        }
        changeColour(colour);

        if(isLightsPaused == 0)
        {
            for(int i = 0; i < delay; i++)
            {
                pollUART();

                int s1now = GPIO_PORTF_DATA_R & 0x10;
                int s2now = GPIO_PORTF_DATA_R & 0x01;
                if(s1now == 0 &&
                   s2now == 0 &&
                   (switch1Value != 0 || switch2Value != 0))
                {
                    waiting = 0;
                    waitCount = 0;

                    isLightsPaused = 1;
                    sendStatus();
                }
                else if(s1now == 0 &&
                        switch1Value != 0 &&
                        isLightsPaused == 0)
                {
                    waiting = 1;
                    waitCount = 0;
                }
                else if(s2now == 0 &&
                        switch2Value != 0 &&
                        isLightsPaused == 0)
                {
                    waiting = 2;
                    waitCount = 0;
                }
                if(waiting != 0)
                {
                    waitCount++;

                    if(waitCount >= WAIT_COUNT)
                    {
                        if(waiting == 1)
                        {
                            rate++;

                            if(rate > 7)
                            {
                                rate = 0;
                            }
                            sendStatus();
                        }
                        else if(waiting == 2)
                        {
                            colour++;

                            if(colour > 7)
                            {
                                colour = 0;
                            }
                            changeColour(colour);
                            sendStatus();
                        }

                        waiting = 0;
                        waitCount = 0;
                    }
                }
                switch1Value = s1now;
                switch2Value = s2now;
                if(isLightsPaused)
                {
                    break;
                }


                for(int j = 0; j < 1000; j++)
                {
                }


                updateDisplay();
            }

            if(isLightsPaused == 0)
            {
                GPIO_PORTF_DATA_R = 0x00;


                for(int i = 0; i < delay; i++)
                {
                    pollUART();

                    int s1now = GPIO_PORTF_DATA_R & 0x10;
                    int s2now = GPIO_PORTF_DATA_R & 0x01;
                    if(s1now == 0 &&
                       s2now == 0 &&
                       (switch1Value != 0 || switch2Value != 0))
                    {
                        waiting = 0;
                        waitCount = 0;

                        isLightsPaused = 1;
                        sendStatus();
                    }
                    else if(s1now == 0 &&
                            switch1Value != 0 &&
                            isLightsPaused == 0)
                    {
                        waiting = 1;
                        waitCount = 0;
                    }
                    else if(s2now == 0 &&
                            switch2Value != 0 &&
                            isLightsPaused == 0)
                    {
                        waiting = 2;
                        waitCount = 0;
                    }
                    if(waiting != 0)
                    {
                        waitCount++;

                        if(waitCount >= WAIT_COUNT)
                        {
                            if(waiting == 1)
                            {
                                rate++;

                                if(rate > 7)
                                {
                                    rate = 0;
                                }
                                sendStatus();
                            }
                            else if(waiting == 2)
                            {
                                colour++;

                                if(colour > 7)
                                {
                                    colour = 0;
                                }
                                changeColour(colour);
                                sendStatus();
                            }

                            waiting = 0;
                            waitCount = 0;
                        }
                    }
                    switch1Value = s1now;
                    switch2Value = s2now;
                    if(isLightsPaused)
                    {
                        break;
                    }


                    for(int j = 0; j < 1000; j++)
                    {
                    }


                    updateDisplay();
                }
            }
        }
        else
        {
            pollUART();

            updateDisplay();

            int s1now = GPIO_PORTF_DATA_R & 0x10;
            int s2now = GPIO_PORTF_DATA_R & 0x01;
            if(s1now == 0 &&
               s2now == 0 &&
               (switch1Value != 0 || switch2Value != 0))
            {
                isLightsPaused = 0;
                sendStatus();

                waiting = 0;
                waitCount = 0;
            }
            switch1Value = s1now;
            switch2Value = s2now;
        }
    }
}
