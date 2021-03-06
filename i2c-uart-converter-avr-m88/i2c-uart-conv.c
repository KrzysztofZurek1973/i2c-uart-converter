// ***********************************************************
// Project:		I2C - UART converter
// CPU:			AVR ATmega88(PA)
// Author:		Krzysztof Zurek
// e-mail:		krzzurek@gmail.com
// github:		KrzysztofZurek1973
// instagram:	alfa46.iot
// Date:		2021-10-24
// 				Gdansk, Poland
// ***********************************************************

#include <avr/io.h>              // Most basic include files
#include <avr/interrupt.h>       // Add the necessary ones
#include <util/twi.h>
#include <util/delay.h>
#include <avr/sleep.h>

#include "i2c-uart-conv.h"		//macros and global variables are
								//defined here

// ***********************************************************
// Main program
//
int main(void) {
	sys_init();
	sei();
	sleep_enable();
	error_flags = 0;

	while(1) {
		ENTER_CRITICAL_REGION();
		if (error_flags > 0){
			LED_ERR_PORT ^= _BV(LED_ERR_GPIO);
			error_flags = 0;
		}
		if ((twi_data_ready_to_send == true) && (twi_state == STOPPED)){
			//start sending data by I2C
			twi_data_ready_to_send = false;
			twi_state = SENDING;
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
		}
		LEAVE_CRITICAL_REGION();
		//CPU sleep, take a rest
		sleep_cpu();
	}
	return 0;
}

// ******************************************
// I2C interrupt
ISR(TWI_vect){
	twst = TW_STATUS;

	switch (twst){
	case TW_START:
		//0x08, Master Transmitter, START condition transttited
		TWDR = RECEIVER_ADR << 1;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		break;

	case TW_MT_SLA_ACK:
		//0x18, SLA+W transmitted, ACK received, send data from twi_out_buff
		TWDR = twi_out_buff[twi_out_buff_tail];
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
		break;

	case TW_MT_DATA_ACK:
		//0x28, data transmitted, ACK received
		twi_out_buff_items--;		
		
		if (twi_out_buff_items > 0){
			if (twi_out_buff_tail == (BUFF_LEN_1 - 1)){
				twi_out_buff_tail = 0;
			}
			else{
				twi_out_buff_tail++;
			}
			TWDR = twi_out_buff[twi_out_buff_tail];
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);	
		}
		else{
			//nothing more to send, stop transmission
			twi_data_ready_to_send = false;
			twi_out_buff_head = 0;
			twi_out_buff_tail = 0;

			stop_sent = true;
			if (timer1_running == false){
				timer1_running = true;
				START_TIMER(30); //30us
			}
			//send STOP
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
		}
		break;

	case TW_MT_SLA_NACK:
		//0x20, SLA+W transmitted, NACK received
	case TW_MT_DATA_NACK:
		//0x30, data transmitted, NACK received
		//try repeat transmission after a while
		stop_sent = true;
		if (timer1_running == false){
				timer1_running = true;
				START_TIMER(10000); //10ms
			}
		//send STOP
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
		break;
	
	case TW_MT_ARB_LOST:
		//0x38, arbitration lost in SLA+W or data
      TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
		break;
		
	case TW_SR_ARB_LOST_SLA_ACK:
		//0x68, arbitration lost in SLA+RW, SLA+W received, ACK returned
		twi_state = RECEIVING;
		if (twi_out_buff_items > 0){
			twi_data_ready_to_send = true;
		}
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
		break;
		
	case TW_ST_ARB_LOST_SLA_ACK:
		//0xB0, arbitration lost in SLA+RW, SLA+R received, ACK returned
	case TW_SR_ARB_LOST_GCALL_ACK:
		//0x78, arbitration lost in SLA+RW, general call received, ACK returned
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
		break;
		
	case TW_SR_SLA_ACK:
		//0x60, SLA+W received, ACK returned
		if (twi_state == STOPPED){
			twi_state = RECEIVING;
			if (uart_out_buff_items < BUFF_LEN){
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			}
			else{
				TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWIE)) & ~_BV(TWEA);
			}
		}
		else {
			TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWIE)) & ~_BV(TWEA);
		}
		break;

	case TW_SR_DATA_ACK:
		//0x80, slave: data received, ACK sent
		if ((uart_out_buff_items == 0) && (udr_is_empty == true)){
			//write data directly into uart UDR register
			UDR0 = TWDR;
			udr_is_empty = false;
			UDRE_ISR_ENABLE();
			TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		}
		else{
			//write data into UART out buffer
			uart_out_buff[uart_out_buff_head] = TWDR;   			
			//update head index
			if (uart_out_buff_head == (BUFF_LEN - 1)){
				uart_out_buff_head = 0;
			}
			else{
				uart_out_buff_head++;
			}
			uart_out_buff_items++;			

			if (uart_out_buff_items < BUFF_LEN){
				//there is available space in buffer
				TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
			}
			else{
				//no space in buffer, send NOT ACK for next byte
				error_flags |= 0x01; //ERROR: uart out buffer is full
				twi_state = BLOCKED_RECEIVING;
				TWCR = (_BV(TWINT) | _BV(TWEN) | _BV(TWIE)) & ~_BV(TWEA);
			}
		}
		break;

	case TW_SR_DATA_NACK:
		//0x88, data received, NACK returned and other states
		temp = TWDR;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;

	case TW_SR_STOP:
		//0xA0, stop or repeated start condition received while selected
		twi_state = STOPPED;
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
		break;
	
	default:
		//all other events: go back to not addressed mode
		stop_sent = true;
		if (timer1_running == false){
			timer1_running = true;
			START_TIMER(30);
		}
		TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO) | _BV(TWEA) | _BV(TWIE);
	}
}


//*****************************************************
// UART UDR empty interrupt
ISR(USART_UDRE_vect){
	LED_PORT ^= _BV(LED_GPIO);

	if (uart_out_buff_items > 0){
		//there is something to send
		UDR0 = uart_out_buff[uart_out_buff_tail];
		uart_out_buff_items--;
		if (uart_out_buff_items > 0){
			//update tail index
			if (uart_out_buff_tail == (BUFF_LEN - 1)){
				uart_out_buff_tail = 0;
			}
			else{
				uart_out_buff_tail++;
			}
		}
		else{
			//nothing more to send
			uart_out_buff_head = 0;
			uart_out_buff_tail = 0;
		}
	}
	else{
		//nothing more to send, disable UDRE ISR
		UDRE_ISR_DISABLE();
		udr_is_empty = true;
	}
}


//************************************************
//UART ISR: data received
ISR(USART_RX_vect){
	uint8_t data = UDR0;

	LED_PORT ^= _BV(LED_GPIO);
	RESET_TIMER();
	if (twi_out_buff_items < BUFF_LEN_1){
		twi_out_buff[twi_out_buff_head] = data;
		twi_out_buff_items++;
		//update head index
		if (twi_out_buff_head == (BUFF_LEN_1 - 1)){
			twi_out_buff_head = 0;
		}
		else{
			twi_out_buff_head++;
		}

		if (twi_out_buff_items >= TRIG_LEVEL){
			//start sending data
			twi_data_ready_to_send = true;
			STOP_TIMER();
		}
		else{
			//start timer and wait for the next byte
			START_TIMER(1000); //wait 1ms
		}
	}
	else{
		//buffer full, discard received data
		error_flags |= 0x02;
	}
}

//Timer1 compare A interrrupt
//time out when receiving UART data
ISR(TIMER1_COMPA_vect){
	bool temp = true;
	uint8_t twcr;
	
	if (twi_out_buff_items > 0){
		twi_data_ready_to_send = true;
	}
	if (stop_sent == true){
		stop_sent = false;
		if (twi_state == SENDING){
			twcr = TWCR;
			if ((twcr & (1 << TWSTO)) == 0){
				twi_state = STOPPED;
			}
			else {
				START_TIMER(30);
				stop_sent = true;
				timer1_running = true;
				temp = false;
			}
		}
	}
	if (temp == true) {
		timer1_running = false;
		STOP_TIMER();
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
	DDRC = (1 << PC0);// | (1 << PC1) | (1 << PC5) | (1 << PC4); //output
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

	//USART enable
	UBRR0H = (unsigned char)(MYUBRR >> 8);
	UBRR0L = (unsigned char)MYUBRR;
	//Enable receiver and transmitter */
	UCSR0B = (1 << RXEN0)|(1 << TXEN0)|(1 << RXCIE0);
	/* Set frame format: 8data, 1stop bit */
	UCSR0C = (3 << UCSZ00);

	//Timer1, compare A interrupt
	OCR1A = 1000; //wait 1ms after every byte
	TIMSK1 |= (1 << OCIE1A); //enable output compare interrupt

	//Timer 2 - blinking LED
#if TEST==1
	TCCR2B = 0x02; //prescaler 8 for tests
#else
	TCCR2B = 0x07; //prescale 1024
#endif
	TIMSK2 |= (1 << TOIE2);
}



