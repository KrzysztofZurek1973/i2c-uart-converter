// ***********************************************************
// Project:		test transmitter for testing I2C - UART converter
// CPU:			AVR ATmega88(PA)
// Author:		Krzysztof Zurek
// Company:		Alfa46, www.alfa46.com
// e-mail:		krzzurek@gmail.com
// github:		KrzysztofZurek1973
// instagram:	alfa46.iot
// Date:		2021-09-25
// 				Gdansk, Poland
//
// uC sends constantly the following message:
// {N--abc..xyz}
//	where:
//		N - incremented 16 bit number
//		abc..xyz - ASCII letters form 'a' to 'z'
// ***********************************************************

#include <avr/io.h>              // Most basic include files
#include <avr/interrupt.h>       // Add the necessary ones
#include <util/twi.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <string.h>
#include <stdlib.h>

#define TEST 0

#define SLA_ADR 0x0A	//I2C slave address
#define SEND_ADR 0x03	//I2C receiver address
#define BUFF_LEN 64		//receive buffer length (data received by I2C)
#define LETTERS 26
#define LONG_WAIT 20
#define LINES_MAX 1000

#define true 1
#define false 0

typedef unsigned char bool;

//macros
/** This macro saves the global interrupt status. */
#define ENTER_CRITICAL_REGION()         {uint8_t sreg = SREG; cli()
/** This macro restores the global interrupt status. */
#define LEAVE_CRITICAL_REGION()         SREG = sreg;}
//enable/disable UDRE interrupt
#define UDRE_ISR_ENABLE()	UCSRB |= (1 << UDRIE)
#define UDRE_ISR_DISABLE()	UCSRB &= ~(1 << UDRIE)
//Timer 1
#define RESET_TIMER()	TCNT1 = 0;\
		TIFR |= (1 << OCF1A)

//start timer1 for us microseconds
#define START_TIMER(us)	TCNT1 = 0;\
		TIFR |= (1 << OCF1A);\
		TCCR1B |= (1 << CS11);\
		OCR1A = us

#define STOP_TIMER()	TCCR1B &= ~0x07;\
		TIFR |= (1 << OCF1A)

#define LED_GPIO PC0
#define LED_PORT PORTC

typedef enum {NO_START = 0, NEW_PACKET = 1, START_ONLY = 2} start_twi_t;

//define blinking LED period (much shorter for tests in VMLab)
#if TEST==1
#define T2_MAX 2
#else
#define T2_MAX 60
#endif

// Define here the global static variables
uint8_t twst;
volatile uint16_t main_counter = 0, line_cnt = 0;
start_twi_t twi_start = NEW_PACKET;
//buffer for data sending by I2C
char twi_out_buff[BUFF_LEN];	
volatile uint8_t byte_nr = 0;

uint8_t t2_counter = 0;
uint16_t long_wait_cnt;
uint8_t temp;
bool new_line = false;

//function prototypes
void sys_init(void);

/***********************************************************
*
*	Main loop
*
**********************************************************/
int main(void) {
	uint8_t n = 0, n1 = 0, i;

	sys_init();
	sei();
	sleep_enable();

	memset(twi_out_buff, 0, BUFF_LEN);
	utoa(main_counter++, twi_out_buff, 10);
	n = strlen(twi_out_buff);
	n1 = 7 - n;
	if (n1 > 0){
		for (i = n; i < 7; i++){
			twi_out_buff[i] = '-';
		}
	}
	//fill buffer with ascii low letters
	for (i = 0; i < LETTERS; i++){
		twi_out_buff[i + 7] = 0x61 + i;
	}
	twi_out_buff[i + 7] = '\n';

	while(1) {
		if (twi_start > NO_START){
			if (twi_start == NEW_PACKET){
				//start sending data by I2C
				ENTER_CRITICAL_REGION();
				byte_nr = 0;
				line_cnt = 0;
				LEAVE_CRITICAL_REGION();
			}
			twi_start = NO_START;
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
		}
		
		if (new_line == true){
			ENTER_CRITICAL_REGION();
			//ascii characters
			new_line = false;
			for (i = 0; i < LETTERS; i++){
				twi_out_buff[i + 7] = 0x61 + i;
			}
			twi_out_buff[i + 7] = '\n';
			//line number
			memset(twi_out_buff, 0, 7);
			utoa(main_counter++, twi_out_buff, 10);
			n = strlen(twi_out_buff);
			n1 = 7 - n;
			if (n1 > 0){
				for (i = n; i < 7; i++){
					twi_out_buff[i] = '-';
				}
			}
			LEAVE_CRITICAL_REGION();
			
			if (main_counter >= 60000){
				main_counter = 0;
			}
		}
		sleep_cpu();
	}
	return 0;
}


// ******************************************
// I2C interrupt
ISR(TWI_vect){
	uint8_t temp_byte;
	twst = TW_STATUS;

	switch (twst){
	case TW_START:
		//ascii_counter = 0;
		//0x08, Master Transmitter, START condition transttited
		TWDR = SEND_ADR << 1;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		break;

	case TW_MT_SLA_ACK:
		TWDR = twi_out_buff[byte_nr];
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		break;

	case TW_MT_DATA_ACK:
		//0x18, SLA+W transmitted, ACK received, send data from twi_out_buff
		//0x28, data transmitted, ACK received
		byte_nr++;
		temp_byte = twi_out_buff[byte_nr];
		
		if (temp_byte != 0){
			TWDR = twi_out_buff[byte_nr];	
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		}
		else{
			new_line = true;
			line_cnt++;
			byte_nr = 0;
			if (line_cnt >= LINES_MAX){
				START_TIMER(10000);
				long_wait_cnt = LONG_WAIT;
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWIE);
			}
			else{
				TWDR = twi_out_buff[byte_nr];	
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
			}
		}
		break;

	default:
		if (twst == TW_MT_DATA_NACK){
			if ((twi_out_buff[byte_nr] != '\n') &&
				(twi_out_buff[byte_nr] != 0)){
				twi_out_buff[byte_nr] = '*';
			}
		}
		else if (twst == TW_MT_SLA_NACK){
			if ((twi_out_buff[byte_nr] != '\n') &&
				(twi_out_buff[byte_nr] != 0)){
				twi_out_buff[byte_nr] = '=';
			}
		}
		START_TIMER(10000);
		//try to repeat transmittion after a while
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWIE);
		break;
	}
}


//Timer1 compare A interrrupt
ISR(TIMER1_COMPA_vect){
	if (long_wait_cnt == 0){
		//start I2C after waiting for slave to be ready
		twi_start = START_ONLY;
		STOP_TIMER();
	}
	else{
		long_wait_cnt--;
		if (long_wait_cnt == 0){
			//start I2C for sending new packet
			twi_start = NEW_PACKET;
			STOP_TIMER();
		}
	}
}


//Timer2 compare A interrrupt
//Blinking LED period
ISR(TIMER2_OVF_vect){
	t2_counter++;
	if (t2_counter == T2_MAX){
		t2_counter = 0;
		LED_PORT ^= _BV(LED_GPIO);
	}
}


/********************************************
 System initialization
 *********************************************/
void sys_init(void){

	/**********   IO init   *********/
	/* PORTC ----------------------------------
	 */
	//PC0: Blinking LED
	//PC1: Error LED
	PORTC |= 0xFF;
	DDRC = (1 << PC0);//|(1 << PC4)|(1 << PC5); //output
	/* external interrupts */
	/* INT0: button, INT1: encoder */
	//EIFR |= (1 << INTF0)|(1 << INTF1);	//clear INT flags
	//EICRA |= (1 << ISC11)|(1 << ISC01); //INT1 - fall, INT0 - fall
	//EIMSK |= (1 << INT1)|(1 << INT0);
	//other settings to make power consumption as low as possible
	//set analog comparator down
	ACSR |= (1 << ACD);
	//switch off ADC
	ADCSRA &= ~(1 << ADEN);
	//switch off SPI interface
	SPCR &= ~(1 << SPE);

	//switch on I2C interface
	TWSR = 0;				//prescaler 1
	TWBR = 32;				//I2C frequency = 100k
	TWAR = SLA_ADR << 1;
	TWCR |= (1 << TWEN) | _BV(TWIE) | _BV(TWEA);	//enable TWI interface

	//Timer1, compare A interrupt
	//TCCR1B |= (1 << CS11); //prescaler 8
	OCR1A = 1000; //wait 1ms after every byte
	TIMSK |= (1 << OCIE1A); //enable output compare interrupt

	//Timer 2 - blinking LED
#if TEST==1
	TCCR2 = 0x02; //prescaler 8 for tests
#else
	TCCR2 = 0x07; //prescale 1024
#endif
	TIMSK |= (1 << TOIE2);
}
