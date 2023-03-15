#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

int pressed = 0;

ISR(INT0_vect) {
	pressed = !pressed;
	if (pressed)
		PORTB ^= (1 << PB0);
	_delay_ms(20);
	EIFR |= (1 << INTF0);
}

int main() {
	//Set B0 (LED 1) to output
	DDRB |= (1 << DDB0);
	//Set INT0 to generate interrupt request on logical change
	// EICRA |= (1 << ISC00) | (1 << ISC01);
	EICRA |= (1 << ISC00);
	//Enable external interrupt on INT0
	EIMSK |= (1 << INT0);
	//Enable interrupts globally on status register
	SREG |= (1 << 7);
	while (1) {}
}
