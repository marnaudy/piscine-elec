#include <avr/io.h>
#include <util/delay.h>

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

int main() {
	//Set direction of B0, B1, B2, and B4 to output
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	//Set direction of D2 and D4 to output
	DDRD &= ~((1 << DDD2) | (1 << DDD4));
	unsigned int n = 0;
	display(n);
	while (1) {
		//Check input of SW1
		if (!(PIND & (1 << PD2))) {
			n++;
			display(n);
			//Wait 20ms to avoid bounce on push
			_delay_ms(20);
			// //Wait until button is released
			while (!(PIND & (1 << PD2))) {}
			// //Wait 20ms to avoid bounce on release
			_delay_ms(20);
		}
		//Check input of SW2
		if (!(PIND & (1 << PD4))) {
			n--;
			display(n);
			//Wait 20ms to avoid bounce on push
			_delay_ms(20);
			//Wait until button is released
			while (!(PIND & (1 << PD4))) {}
			//Wait 20ms to avoid bounce on release
			_delay_ms(20);
		}
	}
}
