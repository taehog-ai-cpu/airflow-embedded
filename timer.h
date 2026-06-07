// timer.h
#ifndef TIMER_H_
#define TIMER_H_

#include <stdint.h>

extern volatile uint16_t timer_sec;
extern volatile uint8_t sec_flag;

void timer0_init(void);

#endif