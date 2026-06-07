Motor Timer Controller (AVR)
프로젝트 개요

AVR 마이크로컨트롤러 기반의 모터 타이머 제어 시스템입니다.

LCD를 통해 현재 동작 상태와 남은 시간을 표시하며, 버튼 입력을 통해 모터 출력 세기와 타이머를 제어할 수 있습니다.

주요 기능
PWM 기반 모터 속도 제어
LCD 16x2 상태 표시
타이머 카운트다운 기능
버튼을 이용한 제어
인터럽트 기반 1초 타이머
동작 완료 시 자동 정지
하드웨어 구성
MCU
AVR ATmega 계열 (16MHz)
입출력 구성
기능	핀
LCD RS	PC0
LCD E	PC1
LCD D4~D7	PC4~PC7
PWM 출력	PB5 (OC1A)
버튼 1	PG0
버튼 2	PG1
버튼 3	PG2
버튼 4	PG3
버튼 5	PG4
동작 모드
모드	PWM 출력
STOP	0
LOW	300
MED	600
HIGH	950

PWM 주파수는 Timer1 Fast PWM 모드로 생성됩니다.

버튼 기능
버튼	기능
PG0	모드 변경 (STOP → LOW → MED → HIGH)
PG1	시작/일시정지
PG2	30초 증가
PG3	30초 감소
PG4	시스템 초기화
LCD 표시
기본 화면
MODE: HIGH
TIME 00:45 RUN:1
타이머 종료
>>> FINISHED <<<
TIME 00:00
소프트웨어 구조
main.c

주요 기능

LCD 제어
버튼 입력 처리
PWM 제어
UI 업데이트
메인 루프

주요 함수

lcd_init()
input_task()
timer_task()
ui_task()
motor_task()
timer.c

Timer0 오버플로 인터럽트를 사용하여 1ms 시스템 틱 생성

ISR(TIMER0_OVF_vect)

기능

1ms 카운트
1000ms마다 sec_flag 설정
1초 단위 타이머 동작
timer.h

타이머 관련 변수와 함수 선언

extern volatile uint16_t timer_sec;
extern volatile uint8_t sec_flag;

void timer0_init(void);
프로그램 흐름
시스템 초기화
    ↓
LCD 초기화
    ↓
PWM 초기화
    ↓
Timer0 초기화
    ↓
메인 루프
    ├─ timer_task()
    ├─ input_task()
    ├─ ui_task()
    └─ motor_task()
빌드 환경
AVR-GCC
AVR Libc
Atmel Studio / Microchip Studio

필요 헤더

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
