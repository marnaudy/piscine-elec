#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

uint8_t sw1_pressed = 0;
uint8_t n = 0;
uint8_t *ptr = 0;

void display(int n) {
	//Switch all LEDs off
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	//Deal with third LED not being PORTB3
	if (n & 0b1000) {
		PORTB |= (1 << PB4);
	}
	//Display the nice bits
	PORTB |= n & 0b111;
}

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed) {
		//Temporarily disable interrupts
		SREG &= ~(1 << SREG_I);
		n = eeprom_read_byte(ptr);
		n++;
		display(n);
		eeprom_write_byte(ptr, n);
		SREG |= (1 << SREG_I);
	}
	//Wait for signal to stabilise
	_delay_ms(10);
	//Reset interrupt flag
	EIFR &= (1 << INTF0);
}

int main() {
	//Wait for EEPROM to be ready
	while (SPMCSR & (1 << SELFPRGEN)) {};
	n = eeprom_read_byte(ptr);
	display(n);
	//Set LEDs to ouput
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	//Enable interrupt on logical change of SW1
	SREG |= (1 << SREG_I);
	EIMSK |= (1 << INT0);
	EICRA |= (1 << ISC00);
	while (1) {}
}
