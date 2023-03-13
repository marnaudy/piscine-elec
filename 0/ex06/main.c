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
	unsigned int n = 1;
	int direction = 1;
	while (1) {
		display(n);
		if (direction)
			n *= 2;
		else
			n /= 2;
		if (n == 1)
			direction = 1;
		if (n == 8)
			direction = 0;
		_delay_ms(150);
	}
}
