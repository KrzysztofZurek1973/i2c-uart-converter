#ifndef _I2C_UART_CONV_H
#define _I2C_UART_CONV_H
/**
* 
* I2C <-> UART converter on AVR Atmega88
*
*/
#define TEST 0

#define SLA_ADR 0x03		//I2C slave address
#define RECEIVER_ADR 0x0A	//I2C receiver address
#define BUFF_LEN 256		//receive buffer length (data received by I2C)
#define BUFF_LEN_1 256		//buffer length for data received by UART
#define TRIG_LEVEL 10
//UART
#define FOSC 8000000		//CPU clock Speed
#define BAUD 38400         //UART baudrate
#define MYUBRR 12
#define true 1
#define false 0

typedef unsigned char bool;
typedef unsigned char u8_t;
typedef signed char c8_t;
typedef unsigned int u16_t;
typedef signed int s16_t;
typedef unsigned long u32_t;
typedef signed long s32_t;

//macros
/** This macro saves the global interrupt status. */
#define ENTER_CRITICAL_REGION()         {uint8_t sreg = SREG; cli()
/** This macro restores the global interrupt status. */
#define LEAVE_CRITICAL_REGION()         SREG = sreg;}
//enable/disable UDRE interrupt
#define UDRE_ISR_ENABLE()	UCSR0B |= (1 << UDRIE0)
#define UDRE_ISR_DISABLE()	UCSR0B &= ~(1 << UDRIE0)
//Timer 1
#define RESET_TIMER()	TCNT1 = 0;\
						TIFR1 |= (1 << OCF1A)

//start timer1 for us microseconds
#define START_TIMER(us)	TCNT1 = 0;\
						TIFR1 |= (1 << OCF1A);\
						TCCR1B |= (1 << CS11);\
						OCR1A = us
						
#define STOP_TIMER()	TCCR1B &= ~0x07;\
						TIFR1 |= (1 << OCF1A)

#define LED_GPIO PC0
#define LED_PORT PORTC
#define LED_ERR_GPIO PC1
#define LED_ERR_PORT PORTC
//define blinking LED period (much shorter for tests in VMLab)
#if TEST==1
#define T2_MAX 2
#else
#define T2_MAX 60
#endif

typedef enum {STOPPED, SENDING, RECEIVING, BLOCKED_RECEIVING} twi_state_t;

// Define here the global static variables
u8_t twst;
u8_t twi_byte_cnt = 0;
bool twi_send_start = false;
twi_state_t twi_state = STOPPED;
//buffer for data received by UART and ready for sending by I2C
uint8_t twi_out_buff[BUFF_LEN_1];	
uint16_t twi_out_buff_items = 0;
uint16_t twi_out_buff_head = 0, twi_out_buff_tail = 0;

//buffer for data received by I2C and ready for sending by UART
uint8_t uart_out_buff[BUFF_LEN];
uint16_t uart_out_buff_items = 0;
uint16_t uart_out_buff_head = 0, uart_out_buff_tail = 0;

bool udr_is_empty = true;
uint8_t error_flags; 	//bit 0: uart_out_buffer is full
						//bit 1: twi_out_buffer is full
						//bit 2: I2C error
uint8_t t2_counter = 0;
uint8_t temp;

//function prototypes
void sys_init(void);

#endif
