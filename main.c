#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "timer.h"

/**************** STATE ****************/

typedef enum {
    MODE_STOP = 0,
    MODE_LOW,
    MODE_MED,
    MODE_HIGH
} power_mode_t;

volatile power_mode_t power_mode = MODE_STOP;

volatile uint8_t timer_running = 0;
volatile uint8_t timer_done = 0;

/**************** LCD ****************/

#define LCD_RS PC0
#define LCD_E  PC1

char line1[17];
char line2[17];

char prev1[17] = "";
char prev2[17] = "";

/**************** BUTTON STATE ****************/

uint8_t btn_last = 255;
uint16_t btn_press_time = 0;

/**************** LCD LOW ****************/

void lcd_pulse(void)
{
    PORTC |= (1<<LCD_E);
    _delay_us(1);
    PORTC &= ~(1<<LCD_E);
    _delay_us(100);
}

void lcd_nibble(uint8_t data)
{
    PORTC &= ~((1<<PC4)|(1<<PC5)|(1<<PC6)|(1<<PC7));

    if(data & 1) PORTC |= (1<<PC4);
    if(data & 2) PORTC |= (1<<PC5);
    if(data & 4) PORTC |= (1<<PC6);
    if(data & 8) PORTC |= (1<<PC7);

    lcd_pulse();
}

void lcd_cmd(uint8_t c)
{
    PORTC &= ~(1<<LCD_RS);
    lcd_nibble(c>>4);
    lcd_nibble(c&0x0F);
    _delay_ms(2);
}

void lcd_data(uint8_t d)
{
    PORTC |= (1<<LCD_RS);
    lcd_nibble(d>>4);
    lcd_nibble(d&0x0F);
    _delay_us(50);
}

void lcd_str(char *s)
{
    while(*s) lcd_data(*s++);
}

void lcd_init(void)
{
    DDRC |= (1<<LCD_RS)|(1<<LCD_E)
         | (1<<PC4)|(1<<PC5)|(1<<PC6)|(1<<PC7);

    _delay_ms(50);

    lcd_nibble(0x03); _delay_ms(5);
    lcd_nibble(0x03); _delay_ms(5);
    lcd_nibble(0x03); _delay_ms(5);
    lcd_nibble(0x02);

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

/**************** BUTTON ****************/

uint8_t read_pg(void)
{
    uint8_t g = PING;

    if(!(g & (1<<PG0))) return 0;
    if(!(g & (1<<PG1))) return 1;
    if(!(g & (1<<PG2))) return 2;
    if(!(g & (1<<PG3))) return 3;
    if(!(g & (1<<PG4))) return 4;

    return 255;
}

/**************** PWM ****************/

void pwm_init(void)
{
    DDRB |= (1<<PB5);

    TCCR1A = (1<<COM1A1)|(1<<WGM11);
    TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11);

    ICR1 = 1000;
}

/**************** MOTOR TASK ****************/

void motor_task(void)
{
    if(timer_done || timer_sec == 0)
    {
        OCR1A = 0;
        return;
    }

    switch(power_mode)
    {
        case MODE_STOP: OCR1A = 0; break;
        case MODE_LOW:  OCR1A = 300; break;
        case MODE_MED:  OCR1A = 600; break;
        case MODE_HIGH: OCR1A = 950; break;
    }
}

/**************** TIMER TASK ****************/

void timer_task(void)
{
    if(sec_flag)
    {
        sec_flag = 0;

        if(timer_running && timer_sec > 0)
            timer_sec--;

        if(timer_sec == 0)
        {
            timer_running = 0;
            timer_done = 1;
        }
    }
}

/**************** INPUT TASK ****************/

void input_task(void)
{
    uint8_t btn = read_pg();

    if(btn != 255)
    {
        btn_press_time++;

        if(btn_last == 255)
        {
            _delay_ms(20); // debounce
        }

        if(btn_last == 255)
        {
            switch(btn)
            {
                case 0:
                    power_mode++;
                    if(power_mode > MODE_HIGH)
                        power_mode = MODE_STOP;
                    break;

                case 1:
                    timer_running ^= 1;
                    timer_done = 0;
                    break;

                case 2:
                    timer_sec += 30;
                    break;

                case 3:
                    if(timer_sec < 30) timer_sec = 0;
                    else timer_sec -= 30;
                    break;

                case 4:
                    power_mode = MODE_STOP;
                    timer_sec = 60;
                    timer_running = 0;
                    timer_done = 0;
                    OCR1A = 0;
                    break;
            }
        }
    }
    else
    {
        btn_press_time = 0;
    }

    btn_last = btn;
}

/**************** UI TASK ****************/

void ui_task(void)
{
    uint8_t m = timer_sec / 60;
    uint8_t s = timer_sec % 60;

    if(timer_done)
    {
        sprintf(line1, ">>> FINISHED <<<");
        sprintf(line2, "TIME 00:00      ");
    }
    else
    {
        switch(power_mode)
        {
            case MODE_STOP: sprintf(line1,"MODE: STOP     "); break;
            case MODE_LOW:  sprintf(line1,"MODE: LOW      "); break;
            case MODE_MED:  sprintf(line1,"MODE: MED      "); break;
            case MODE_HIGH: sprintf(line1,"MODE: HIGH     "); break;
        }

        sprintf(line2,"TIME %02u:%02u RUN:%d", m, s, timer_running);
    }

    if(strcmp(line1, prev1) != 0)
    {
        lcd_cmd(0x80);
        lcd_str(line1);
        strcpy(prev1, line1);
    }

    if(strcmp(line2, prev2) != 0)
    {
        lcd_cmd(0xC0);
        lcd_str(line2);
        strcpy(prev2, line2);
    }
}

/**************** MAIN LOOP ****************/

int main(void)
{
    DDRG = 0x00;
    PORTG = 0xFF;

    lcd_init();
    pwm_init();
    timer0_init();

    sei();

    strcpy(line1, "BOOT READY");
    strcpy(line2, "SYSTEM INIT");
    ui_task();

    while(1)
    {
        timer_task();
        input_task();
        ui_task();
        motor_task();
    }
}