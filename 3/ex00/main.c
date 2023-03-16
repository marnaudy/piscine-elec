#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

void uart_init() {
	//Enable transmitter on USART0
	UCSR0B |= (1 << TXEN0);
	//Keep defaults : async with no parity and 1 stop bit
	//Set character size to 8 bits
	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ00);
	//Set baud rate to UART_BAUDRATE
	UBRR0 = (F_CPU / 8 / UART_BAUDRATE - 1) / 2;
}

void uart_tx(char c) {
	//Wait for USART to be ready to transmit
	while (!(UCSR0A & (1 << UDRE0))) {}
	//Transmit character
	UDR0 = c;
}

ISR(TIMER1_COMPA_vect) {
	uart_tx('Z');
}

int main() {
	uart_init();
	//Enable interrupts globally on status register
	SREG |= (1 << SREG_I);
	//Set timer 1 to CTC mode
	TCCR1B |= (1 << WGM12);
	//Enable compare A interrupt
	TIMSK1 |= (1 << OCIE1A);
	//Set A to F_CPU / 1024 (prescaler for timer 1 clock)
	OCR1A = 15625;
	//Enable timer 0 with 1024 prescaler
	TCCR1B |= (1 << CS10) | (1 << CS12);
	while (1) {}
}
