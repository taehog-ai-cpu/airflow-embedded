#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

/**************** STATE ****************/
typedef enum
{
    MODE_STOP = 0,
    MODE_LOW,
    MODE_MED,
    MODE_HIGH
} power_mode_t;

volatile power_mode_t power_mode = MODE_STOP;

volatile uint8_t timer_running = 0;
volatile uint16_t timer_sec = 0;
volatile uint8_t sec_flag = 0;

volatile uint8_t sub_motor_enable = 0;

/**************** SERVO ****************/
#define SERVO_MIN_DEG      45
#define SERVO_MAX_DEG      135
#define SERVO_CENTER_DEG   90

#define SERVO_MIN_PULSE    1000
#define SERVO_MAX_PULSE    5000

volatile uint8_t servo_angle = SERVO_CENTER_DEG;
static int8_t direction = 1;

/**************** LCD ****************/
#define LCD_RS PC0
#define LCD_E  PC1

char line1[17];
char line2[17];
char prev1[17] = "";
char prev2[17] = "";

/**************** LCD ****************/
void lcd_pulse(void)
{
    PORTC |= (1 << LCD_E);
    _delay_us(1);
    PORTC &= ~(1 << LCD_E);
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

void lcd_cmd(uint8_t cmd)
{
    PORTC &= ~(1 << LCD_RS);

    lcd_nibble(cmd >> 4);
    lcd_nibble(cmd & 0x0F);

    _delay_ms(2);
}

void lcd_data(uint8_t data)
{
    PORTC |= (1 << LCD_RS);

    lcd_nibble(data >> 4);
    lcd_nibble(data & 0x0F);

    _delay_us(50);
}

void lcd_str(char *str)
{
    while(*str)
        lcd_data(*str++);
}

void lcd_init(void)
{
    DDRC |= (1<<LCD_RS)|(1<<LCD_E)
          | (1<<PC4)|(1<<PC5)|(1<<PC6)|(1<<PC7);

    _delay_ms(50);

    lcd_nibble(0x03);
    _delay_ms(5);

    lcd_nibble(0x03);
    _delay_ms(5);

    lcd_nibble(0x03);
    _delay_ms(5);

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

/**************** SERVO ****************/
uint16_t servo_to_pwm(uint8_t deg)
{
    return SERVO_MIN_PULSE +
           ((uint32_t)deg *
           (SERVO_MAX_PULSE - SERVO_MIN_PULSE))
           / 180;
}

/**************** PWM ****************/
void pwm_init(void)
{
    DDRB |= (1 << PB5);   // OC1A
    DDRE |= (1 << PE3);   // OC3A

    // Main Motor
    TCCR1A = (1<<COM1A1) | (1<<WGM11);
    TCCR1B = (1<<WGM13)  | (1<<WGM12) | (1<<CS11);

    ICR1 = 1000;
    OCR1A = 0;

    // Servo Motor
    TCCR3A = (1<<COM3A1) | (1<<WGM31);
    TCCR3B = (1<<WGM33)  | (1<<WGM32) | (1<<CS31);

    ICR3 = 40000;

    OCR3A = servo_to_pwm(servo_angle);
}

/**************** MAIN MOTOR ****************/
void motor_task(void)
{

    switch(power_mode)
    {
        case MODE_STOP:
            OCR1A = 0;
            break;

        case MODE_LOW:
            OCR1A = 300;
            break;

        case MODE_MED:
            OCR1A = 600;
            break;

        case MODE_HIGH:
            OCR1A = 950;
            break;
    }
}

/**************** SERVO TASK ****************/
void sub_motor_task(void)
{
    static uint16_t move_cnt = 0;

    move_cnt++;

    if(move_cnt < 150)
        return;

    move_cnt = 0;

    if(sub_motor_enable)
    {
        servo_angle += direction;

        if(servo_angle >= SERVO_MAX_DEG)
        {
            servo_angle = SERVO_MAX_DEG;
            direction = -1;
        }

        if(servo_angle <= SERVO_MIN_DEG)
        {
            servo_angle = SERVO_MIN_DEG;
            direction = 1;
        }

        OCR3A = servo_to_pwm(servo_angle);
    }
    // sub_motor_enable == 0 이면 그냥 현재 위치 유지
}

/**************** TIMER0 ****************/
void timer0_init(void)
{
    TCCR0 = (1<<WGM01)
          | (1<<CS02)
          | (1<<CS00);

    OCR0 = 249;

    TIMSK |= (1<<OCIE0);
}
ISR(TIMER0_COMP_vect)
{
    static uint16_t cnt = 0;

    cnt++;

    if(cnt >= 511)
    {
        cnt = 0;
        sec_flag = 1;
    }
}
/**************** TIMER ****************/
void timer_task(void)
{
    if(sec_flag)
    {
        sec_flag = 0;

        if(timer_running && timer_sec > 0)
        {
            timer_sec--;

            if(timer_sec == 0)
            {
                timer_running = 0;

                // 모드 초기화
                power_mode = MODE_STOP;

                // 메인 모터 정지
                OCR1A = 0;

                // 서브 모터 정지
                sub_motor_enable = 0;

                // 서보 중앙 복귀
                servo_angle = SERVO_CENTER_DEG;
                OCR3A = servo_to_pwm(servo_angle);

                // 스윙 방향 초기화
                direction = 1;
            }
        }
    }
}

/**************** INPUT ****************/
void input_task(void)
{
    static uint8_t stable_btn = 255;
    static uint8_t last_read = 255;
    static uint8_t debounce_cnt = 0;

    uint8_t btn = read_pg();

    if(btn == last_read)
    {
        if(debounce_cnt < 5)
            debounce_cnt++;
    }
    else
    {
        debounce_cnt = 0;
    }

    if(debounce_cnt >= 5)
    {
        if(btn != stable_btn)
        {
            stable_btn = btn;

            if(btn != 255)
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
                        break;

                    case 2:
                        timer_sec += 60;

                        if(timer_sec > 3600)
                            timer_sec = 60;
                        break;

                    case 3:
                        if(timer_sec == 0)
                            timer_sec = 3600;
                        else if(timer_sec >= 60)
                            timer_sec -= 60;
                        break;

                    case 4:
                        sub_motor_enable ^= 1;
                        break;
                }
            }
        }
    }

    last_read = btn;
}

/**************** UI ****************/
void ui_task(void)
{
    uint8_t min = timer_sec / 60;
    uint8_t sec = timer_sec % 60;

    switch(power_mode)
    {
        case MODE_STOP:
            snprintf(line1,sizeof(line1),"MODE:STOP");
            break;

        case MODE_LOW:
            snprintf(line1,sizeof(line1),"MODE:LOW ");
            break;

        case MODE_MED:
            snprintf(line1,sizeof(line1),"MODE:MED ");
            break;

        case MODE_HIGH:
            snprintf(line1,sizeof(line1),"MODE:HIGH");
            break;
    }

    snprintf(line2,
             sizeof(line2),
             "%02u:%02u A%03u",
             min,
             sec,
             servo_angle);

    if(strcmp(line1, prev1))
    {
        lcd_cmd(0x80);

        lcd_str("                ");
        lcd_cmd(0x80);

        lcd_str(line1);

        strcpy(prev1, line1);
    }

    if(strcmp(line2, prev2))
    {
        lcd_cmd(0xC0);

        lcd_str("                ");
        lcd_cmd(0xC0);

        lcd_str(line2);

        strcpy(prev2, line2);
    }
}

/**************** MAIN ****************/
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

    lcd_cmd(0x80);
    lcd_str(line1);

    lcd_cmd(0xC0);
    lcd_str(line2);

    _delay_ms(1000);

    lcd_cmd(0x01);

    while(1)
    {
        input_task();
        timer_task();
        motor_task();
        sub_motor_task();
        ui_task();
    }
}
