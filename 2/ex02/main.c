#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

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

unsigned int n = 0;

int sw1_pressed = 0;
int sw2_pressed = 0;

ISR(INT0_vect) {
	//Button state has been toggled
	sw1_pressed = !sw1_pressed;
	//Only change LED state if button is pressed
	if (sw1_pressed) {
		n++;
		display(n);
	}
	//Wait for signal to stabilise
	_delay_ms(1);
	//Clear INT0 to remove any queued interrupts from bounce
	EIFR |= (1 << INTF0);
}

ISR(PCINT2_vect) {
	//Button state has been toggled
	sw2_pressed = !sw2_pressed;
	//Only change LED state if button is pressed
	if (sw2_pressed) {
		n--;
		display(n);
	}
	//Wait for signal to stabilise
	_delay_ms(1);
	//Clear INT0 to remove any queued interrupts from bounce
	PCIFR |= (1 << PCIF2);
}


int main() {
	//Set LEDs to output
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	//Enable interrupts
	SREG |= (1 << SREG_I);
	//Enable external interrupt on INT0
	EIMSK |= (1 << INT0);
	//Set INT0 to generate interrupt request on logical change
	EICRA |= (1 << ISC00);
	//Enable interrupt on pin change 20 (alternate mode of PD4 connected to SW2)
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
	while (1) {}
}
