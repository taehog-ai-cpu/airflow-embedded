#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"

volatile uint16_t sys_tick = 0;
volatile uint16_t timer_sec = 60;
volatile uint8_t sec_flag = 0;

ISR(TIMER0_OVF_vect)
{
    TCNT0 = 6; // 1ms

    sys_tick++;

    if(sys_tick % 1000 == 0)
        sec_flag = 1;
}

void timer0_init(void)
{
    TCCR0 = (1<<CS02) | (1<<CS00); // 1024 prescaler
    TCNT0 = 6;
    TIMSK |= (1<<TOIE0);
}