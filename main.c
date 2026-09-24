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
int delay = 2000;

static char uartBuffer[16];
static uint8_t uartIndex = 0;

typedef enum
{
    SW_DISABLED = 0,
    SW_IDLE,
    SW_RUNNING,
    SW_PAUSED
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
void updateRate(int rate){
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
    uint32_t seconds = (centis / 100) % 60;    // 2 digits, wraps every 60 s
    uint32_t ms = centis % 100;                // 2-digit fractional part (centiseconds)

    displayDigit(4, getHEXNumber(seconds / 10));
    delayRefresh(500);

    displayDigit(3, getHEXNumber(seconds % 10) | 0x80);   // bit7 = decimal point segment
    delayRefresh(500);

    displayDigit(2, getHEXNumber(ms / 10));
    delayRefresh(500);

    displayDigit(1, getHEXNumber(ms % 10));
    delayRefresh(500);
}

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
    SYSCTL_RCGCUART_R |= 0x01;

    delayRefresh(10);

    GPIO_PORTA_AFSEL_R |= 0x03;
    GPIO_PORTA_PCTL_R   = (GPIO_PORTA_PCTL_R & 0xFFFFFF00) |
                           GPIO_PCTL_PA0_U0RX | GPIO_PCTL_PA1_U0TX;
    GPIO_PORTA_DEN_R   |= 0x03;
    GPIO_PORTA_AMSEL_R &= ~0x03;

    UART0_CTL_R &= ~UART_CTL_UARTEN;

    // 115200 baud @ 16 MHz
    /*
     * Integer and fractional parts of the baud-rate divisor for 115200 baud at a 16 MHz
        system clock (BRD = 16,000,000 / (16×115200) » 8.68, so integer 8 and fractional
        0.68×64+0.5 » 44).
      */

    UART0_IBRD_R = 8;
    UART0_FBRD_R = 44;

    UART0_LCRH_R = UART_LCRH_WLEN_8;
    UART0_CC_R = 0x0;

    UART0_CTL_R |= (UART_CTL_UARTEN | UART_CTL_TXE | UART_CTL_RXE);
}

void UART0_SendChar(char data)
{
    while (UART0_FR_R & UART_FR_TXFF) { }
    UART0_DR_R = data;
}

void UART0_SendString(const char *str)
{
    while (*str)
    {
        UART0_SendChar(*str);
        str++;
    }
}

int UART0_CharAvail(void)
{
    return ((UART0_FR_R & UART_FR_RXFE) == 0);
}

char UART0_GetChar(void)
{
    while (UART0_FR_R & UART_FR_RXFE) { }
    return (char)(UART0_DR_R & 0xFF);
}

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

void sendStopwatchStatus(void)
{
    const char *label;
    uint32_t seconds = (swCentis / 100) % 60;
    uint32_t ms = swCentis % 100;

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
    UART0_SendChar((char)('0' + (seconds / 10)));
    UART0_SendChar((char)('0' + (seconds % 10)));
    UART0_SendChar('.');
    UART0_SendChar((char)('0' + (ms / 10)));
    UART0_SendChar((char)('0' + (ms % 10)));
    UART0_SendString("\r\n");
}

// Explicit stopwatch actions (mirrors RATE/COLOUR/PAUSE/RESUME style on the
// LED side: each command does one specific thing, not a toggle). Shared by
// both the UART parser and the keypad handler.

void stopwatchEnable(void)
{
    if(swState == SW_DISABLED)
    {
        swState = SW_IDLE;
        swCentis = 0;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch already enabled\r\n");
    }
}

void stopwatchDisable(void)
{
    if(swState != SW_DISABLED)
    {
        swState = SW_DISABLED;
        swCentis = 0;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch already disabled\r\n");
    }
}

void stopwatchStart(void)
{
    if(swState == SW_IDLE)
    {
        swState = SW_RUNNING;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch cannot start from current state\r\n");
    }
}

void stopwatchStop(void)
{
    if(swState == SW_RUNNING || swState == SW_PAUSED)
    {
        swState = SW_IDLE;
        swCentis = 0;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch already stopped\r\n");
    }
}

void stopwatchPause(void)
{
    if(swState == SW_RUNNING)
    {
        swState = SW_PAUSED;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch is not running\r\n");
    }
}

void stopwatchResume(void)
{
    if(swState == SW_PAUSED)
    {
        swState = SW_RUNNING;
        sendStopwatchStatus();
    }
    else
    {
        UART0_SendString("Stopwatch is not paused\r\n");
    }
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
            if(isLightsPaused == 1){
                UART0_SendString("Please Unpause before incrementing Rate");

                return;
            }
            rate = (rate + 1) > 7 ? 0 : rate + 1;
            updateRate(rate);
            sendStatus();
        }
        else if(strcmp(uartBuffer, "COLOUR") == 0)
        {
            if(isLightsPaused == 1){
                            UART0_SendString("Please Unpause before incrementing Color");

                            return;
                        }
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
        else if(strcmp(uartBuffer, "SWENABLE") == 0)
        {
            stopwatchEnable();
        }
        else if(strcmp(uartBuffer, "SWDISABLE") == 0)
        {
            stopwatchDisable();
        }
        else if(strcmp(uartBuffer, "SWSTART") == 0)
        {
            stopwatchStart();
        }
        else if(strcmp(uartBuffer, "SWSTOP") == 0)
        {
            stopwatchStop();
        }
        else if(strcmp(uartBuffer, "SWPAUSE") == 0)
        {
            stopwatchPause();
        }
        else if(strcmp(uartBuffer, "SWRESUME") == 0)
        {
            stopwatchResume();
        }
        else if(strcmp(uartBuffer, "SWSTATUS") == 0)
        {
            sendStopwatchStatus();
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


// Keypad stopwatch control: row 0 (PE0) held low, columns PC4/PC5/PC6
// wired to keys 1/2/3. A falling edge on a column is unambiguous since
// only one row is ever active, so no scanning is needed.

void SysTick_Handler(void);
void GPIOPortC_Handler(void);

void Stopwatch_Init(void)
{
    GPIO_PORTE_DIR_R |= 0x01;      // make PE0 as output
    GPIO_PORTE_ODR_R |= 0x01;     // open-drain row pe0
    GPIO_PORTE_DEN_R |= 0x01;     // digital enable pe0
    GPIO_PORTE_DATA_R &= ~0x01;   // row held low

    GPIO_PORTC_AFSEL_R &= ~0x70;    //pc4,5,6 as gpio
    GPIO_PORTC_AMSEL_R &= ~0x70;
    GPIO_PORTC_DIR_R   &= ~0x70;    // pc4,5,6 as input
    GPIO_PORTC_PUR_R   |= 0x70;
    GPIO_PORTC_DEN_R   |= 0x70;

    GPIO_PORTC_IS_R  &= ~0x70;    // edge sensitive rather than level
    GPIO_PORTC_IBE_R &= ~0x70;    // single edge
    GPIO_PORTC_IEV_R &= ~0x70;    // falling edge
    GPIO_PORTC_ICR_R  = 0x70;       // clear previous interrupt flags
    GPIO_PORTC_IM_R  |= 0x70;   // enable interrupt for 456

    NVIC_EN0_R |= (1 << 2);       /*IRQ 2 = GPIO Port C//Enables IRQ number 2 in the NVIC, which on this chip is the GPIO Port C interrupt
    — without this, the pin-level interrupt flag would set but the CPU would never
    actually jump to GPIOPortC_Handler*/

    // SysTick: 10 ms tick @ 16 MHz system clock
    NVIC_ST_CTRL_R    = 0; //disable systick as we are changing configuratioon
    NVIC_ST_RELOAD_R  = 160000 - 1;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R    = 0x07;
}

void SysTick_Handler(void)
{
    msTicks++;

    if(swState == SW_RUNNING)
    {
        swCentis++;
        if(swCentis >= 6000)   // wrap after 60 s
        {
            swCentis = 0;
        }
    }
}

void GPIOPortC_Handler(void)
{
    uint32_t status = GPIO_PORTC_MIS_R & 0x70;//which pin trigger interrupt

    if((status & 0x10) && (msTicks - lastPress[0] > DEBOUNCE_TICKS))   // key 1
    {
        lastPress[0] = msTicks;
        if(swState == SW_DISABLED) stopwatchEnable();
        else stopwatchDisable();
    }

    if((status & 0x20) && (msTicks - lastPress[1] > DEBOUNCE_TICKS))   // key 2
    {
        lastPress[1] = msTicks;
        if(swState == SW_IDLE) stopwatchStart();
        else if(swState == SW_RUNNING || swState == SW_PAUSED) stopwatchStop();
    }

    if((status & 0x40) && (msTicks - lastPress[2] > DEBOUNCE_TICKS))   // key 3
    {
        lastPress[2] = msTicks;
        if(swState == SW_RUNNING) stopwatchPause();
        else if(swState == SW_PAUSED) stopwatchResume();
    }

    GPIO_PORTC_ICR_R = 0x70;
}


int main(void)
{
    int switch1Value = 1;
    int switch2Value = 1;

    int waiting = 0;
    int waitCount = 0;

    SYSCTL_RCGC2_R |= 0x00000037;   // Ports A, B, C, E, F

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
    UART0_SendString("Stopwatch commands: SWENABLE SWDISABLE SWSTART SWSTOP SWPAUSE SWRESUME SWSTATUS\r\n");
    UART0_SendString("Keypad: 1=Enable/Disable  2=Start/Stop  3=Pause/Resume\r\n");
    sendStatus();


    while(1)
    {
        updateRate(rate);
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
                            updateRate(rate);
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
                                updateRate(rate);
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
