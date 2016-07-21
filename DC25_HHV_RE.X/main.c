/* MIT License
 *
 * Copyright (c) 2017 KBEmbedded and DEF CON Hardware Hacking Village
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



/*
 * Basic pinmap reference
 * RA0: TXD
 * RA1: RXD
 * RA2: Analog button input
 * RA3: Unused
 * RA4: Red LED#
 * RA5: Green LED#
 */

#include <xc.h>

//#define ECHO_UART             // Uncomment to enable echo of recv'ed chars

#define _XTAL_FREQ 16000000   // Oscillator frequency.

#pragma config FOSC = INTOSC  // INTOSC oscillator: I/O function on CLKIN pin.
#pragma config WDTE = OFF     // Watchdog Timer disable.
#pragma config PWRTE = OFF    // Power-up Timer enbable.
#pragma config MCLRE = ON     // MCLR/VPP pin function is MCLR.
#pragma config CP = OFF       // Program memory code protection disabled.
#pragma config BOREN = ON     // Brown-out Reset enabled.
#pragma config CLKOUTEN = OFF // CLKOUT function is disabled; I/O or oscillator function on the CLKOUT pin.
#pragma config WRT = OFF      // Flash Memory Write protection off.
#pragma config STVREN = ON    // Stack Overflow or Underflow will cause a Reset.
#pragma config BORV = LO      // Brown-out Reset Voltage (Vbor), low trip point selected.
#pragma config LVP = OFF      // High-voltage on MCLR/VPP must be used for programming.

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

volatile uint8_t uart_byte;
volatile uint8_t adc_byte;
volatile bit adc_flag;
volatile bit uart_flag;
volatile bit blink_flag;
volatile bit newline_flag;
bit did_something = 0;  //Global so it is optimized with other bits
bit unlocked;
unsigned char cmdstring[9] = {0};
uint8_t kpbytes[9];
uint8_t kpcnt;

/* Cheaper than #defines */
const uint8_t LED_R = 4;
const uint8_t LED_G = 5;

void init_pic() {
  OSCCON = 0b01111010; // 16 Mhz oscillator.
  ANSELA = 0b00000100; // Analog enable on RA2 for button input
  TRISAbits.TRISA2 = 1;
  
  /* LEDs as outputs, active low */
  PORTAbits.RA4 = 1;
  PORTAbits.RA5 = 1;
  TRISAbits.TRISA4 = 0;
  TRISAbits.TRISA5 = 0;
}

void init_uart() {
  TRISAbits.TRISA1 = 1; // UART RX pin an input.
  TRISAbits.TRISA0 = 0; // UART TX pin an output.

  // 9600 bps, 16 MHz oscillator.
  SPBRGH = 0x01;
  SPBRGL = 0xA0;

  // 16-bit Baud Rate Generator
  BAUDCONbits.BRG16 = 1;

  TXSTAbits.TXEN = 1; // Transmit enabled.
  TXSTAbits.SYNC = 0; // Enable asynchronous mode.
  TXSTAbits.BRGH = 1; // High speed.

  RCSTAbits.SPEN = 1; // Enable serial port.

}

void init_adc(void) {
    /* ANSEL/TRIS already set by init_pic() */
    
    /* Set channel selection, AN2 */
    ADCON0bits.CHS = 0b00010;
    
    /* Set VREF to VDD */
    ADCON1bits.ADPREF = 0;
    
    /* Set conversion clock, Fosc/64, ~4 us conversion time */
    ADCON1bits.ADCS = 0b110;
    
    /* 10-bit left-justified */
    ADCON1bits.ADFM = 0;
    
    /* Enable ADC */
    ADCON0bits.ADON = 1;
    
    /* Enable ADC conversion interrupt */
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;
    
    /* Enable timed conversion, ~10ms, from Timer0 */
    ADCON2bits.TRIGSEL = 0b0011;
    
    adc_flag = 0;
}

/* Set up Timer0 with 1:128 prescaler
 * At 16 MHz, this is roughly 8.2 ms, a close enough timing for ADC
 */
void init_timer0(void) 
{
    TMR0 = 0;
    OPTION_REGbits.PSA = 0;
    OPTION_REGbits.PS = 0b110;
    OPTION_REGbits.TMR0CS = 0;
}

void putch(unsigned char byte) {
  // Wait until no char is being held for transmission in the TXREG.
  while (!PIR1bits.TXIF) {
    continue;
  }
  // Write the byte to the transmission register.
  TXREG = byte;
}

void interrupt interrupt_handler() {
  if (PIR1bits.RCIF && PIE1bits.RCIE) {
    // EUSART receiver enabled and unread char in receive FIFO.
    if (RCSTAbits.OERR) {
      // Overrun error.
      RCSTAbits.CREN = 0;          // Clear the OERR flag.
      uart_byte = RCREG; // Clear any framing error.
      RCSTAbits.CREN = 1;
    } else {
      uart_byte = RCREG;
      if(uart_byte == '\r') newline_flag = 1;
      else uart_flag = 1;
    }
  }
  
  /* ADC, PIR1bits.ADIF must be manually cleared*/
  if (PIR1bits.ADIF && PIE1bits.ADIE) {
      PIR1bits.ADIF = 0;
      adc_byte = ADRESH;
      adc_flag = 1;
  }
}

void led_blink(uint8_t led) {
    LATA ^= (uint8_t)(1 << led);
    __delay_ms(50);
    LATA ^= (uint8_t)(1 << led);
}

void led_toggle(uint8_t led) {
    LATA ^= (uint8_t)(1 << led);
}

void parsecmd(void) 
{
        uint8_t cnt;

        if (!strcmp(cmdstring, "help")) {
            printf("Available commands:\r\n");
            printf("help\r\n");
            printf("ledtest\r\n");
            printf("Listing of debug commands is disabled\r\n");
            did_something = 1;
        }
        
        if (!strcmp(cmdstring, "ledtest")) {
            for(cnt = 10; cnt != 0; cnt--) {
                led_blink(LED_G);
                led_blink(LED_R);
            }
            did_something = 1;
        }

        if (!strcmp(cmdstring, "lockit")) {
            unlocked = 0;
            did_something = 1;
        }
        
        if (!strcmp(cmdstring, "unlockit")) {
            unlocked = 1;
            did_something = 1;
        }
        
        if(!did_something) {
            printf("Unknown command\r\n");
        }
        did_something = 0;
}

/* I think the best math for this will be 8 "chars", 4 inputs, 4096 total, 8^4
 * combinations.  If I focus on short timeout between attempts, can thwart brute
 * forcing.  
 * 
 * Average input time is about 3 seconds with medium speed, for 8 digits
 * We assume a brute forcer has to go through 50% of the key space to get the 
 *   correct answer.
 * This means 2048 total guesses.
 * With 10 s padding after p/w input, this averages out to 7 hours, assuming the 
 *   above statements.*/
void kp_parse(uint8_t data) {
    static bit repeated;
    static bit valid_press;
    
    if (data < 0x40) data = '1';
    if (data < 0x95 && data > 0x3F) data = '2';
    if (data < 0xb4 && data > 0x94) data = '3';
    if (data < 0xe5 && data > 0xb3) data = '4';
    if (data > 0xe4) data = 0; //Released
    
    if (repeated && (kpbytes[kpcnt] == data)) {
        repeated = 0;
        valid_press = 1;
        led_toggle(LED_G);        
    } 
    
    if (!valid_press && data) {
        kpbytes[kpcnt] = data;
        repeated = 1;
    }
    
    if (valid_press && data == 0) {
        valid_press = 0;
        kpcnt++;
        led_toggle(LED_G);
    }
}

/* Compares the global kpbytes to *string
 * If any character is wrong, delay for 1 us, if correct, then delay 2 us
 * This becomes an exaggerated side channel timing attack.
 * 
 * HARD_MODE will delay differently based on correct or incorrect
 * !HARD_MODE will fail instantly on first incorrect character
 */
#ifdef HARD_MODE
uint8_t kp_compare(const unsigned uint8_t *string)
{
    uint8_t i;
    uint8_t success = 1;
    for (i = 0; i != 8; i++) {
        if (kpbytes[i] == *string++) {
            __delay_us(2);
        } else {
            __delay_us(1);
            success = 0;
        }
    }
    return success;
}
#else
/* Maybe run through the string in reverse just to throw people off? */
uint8_t kp_compare(const unsigned char *string)
{
    uint8_t i;
    for (i = 0; i != 8; i++) {
        if (kpbytes[i] != *string++) {
            return (uint8_t)0;
        }
        __delay_ms(1);
    }
    return (uint8_t)1;
}
#endif

int main() {
    uint8_t recvcnt = 0;
    uint8_t i;
    init_pic();
    init_uart();
    init_adc();
    init_timer0();

    __delay_ms(1000);
    printf("SecurLock mechanism\r\n");
    printf("Build Features: Pre-set pass, code protect, debug output disabled.\r\n");
    printf("Rev. 1.43.2.341.4\r\n");
    printf("Initializing\r\n");
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    printf("Starting ADC");
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    printf("Setting up serial line buffers");
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    putch('.');
    led_toggle(LED_R);
    __delay_ms(1000);
    led_toggle(LED_R);
    printf("\r\nSystem started\r\n");
    
    // Enable serial RX and general interrupts
    RCSTAbits.CREN = 1;
    INTCONbits.GIE = 1;
    INTCONbits.PEIE = 1;

    // Enable the receive interrupt.
    PIE1bits.RCIE = 1;

    uart_flag = 0;
    
    while (1) {
        if (uart_flag) {
#ifdef ECHO_UART
            putch(uart_byte);
#endif
            uart_flag = 0;
            if(recvcnt != 8) cmdstring[recvcnt++] = uart_byte;
        }
    
        if (adc_flag) {
            adc_flag = 0;
            kp_parse(adc_byte);
            
            /* If we have a full word input, check it, disable interrupts
             * Because this could be a long function and there is no reason
             * to leave them enabled anyway 
             */
            if (kpcnt == 8) {
                PIE1bits.ADIE = 0;
                PIE1bits.RCIE = 0;
                if (unlocked) {
                    if (kp_compare("41241342")) {
                        unlocked = 0;
                        LATA |= (uint8_t)(1 << LED_G);
                    } else {
                        for (i = 10; i != 0; i--) {
                            led_blink(LED_R);
                            __delay_ms(50);
                        }
                    }
                } else {
                    /* In case anyone thinks the arbitrary rev number is the code */
                    if (kp_compare("14323414")) {
                        printf("lol, nice try though!\r\n");
                    }
                    
                    if (kp_compare("33124121")) {
                        unlocked = 1;
                        LATA &= ~(uint8_t)(1 << LED_G);
                    } else {
                        for (i = 100; i != 0; i--) {
                            led_blink(LED_R);
                            __delay_ms(50);
                        }
                    }
                }
                
                /* Clear our the kpbyte buffer and re-enable ADC/UART RX */
                while (--kpcnt) kpbytes[kpcnt] = 0;
                
                PIR1bits.RCIF = 0;
                PIE1bits.RCIE = 1;
                
                PIR1bits.ADIF = 0;
                PIE1bits.ADIE = 1;
            }

        }
    
        if (newline_flag) {
            parsecmd();
        
            /* Clean up buffer and reset for next command */
            newline_flag = 0;
            while(recvcnt) cmdstring[--recvcnt] = 0;
        }
    }
  return (EXIT_SUCCESS);
}
