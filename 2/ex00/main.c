#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

int pressed = 0;

ISR(INT0_vect) {
	//Button state has been toggled
	pressed = !pressed;
	//Only change LED state if button is pressed
	if (pressed)
		PORTB ^= (1 << PB0);
	//Wait for signal to stabilise
	_delay_ms(1);
	//Clear INT0 to remove any queued interrupts from bounce
	EIFR |= (1 << INTF0);
}

int main() {
	//Set B0 (LED 1) to output
	DDRB |= (1 << DDB0);
	//Set INT0 to generate interrupt request on logical change
	EICRA |= (1 << ISC00);
	//Enable external interrupt on INT0
	EIMSK |= (1 << INT0);
	//Enable interrupts globally on status register
	SREG |= (1 << SREG_I);
	while (1) {}
}
