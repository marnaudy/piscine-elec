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
	int sw1_state = (PIND & (1 << PD2));
	int sw2_state = (PIND & (1 << PD4));
	while (1) {
		//Check change of state of SW1
		if ((PIND & (1 << PD2)) != sw1_state) {
			sw1_state = (PIND & (1 << PD2));
			if (!sw1_state) {
				n++;
				display(n);
			}
			//Wait 20ms to avoid bounce
			_delay_ms(20);
		}
		//Check change of state of SW2
		if ((PIND & (1 << PD4)) != sw2_state) {
			sw2_state = (PIND & (1 << PD4));
			if (!sw2_state) {
				n--;
				display(n);
			}
			//Wait 20ms to avoid bounce
			_delay_ms(20);
		}
	}
}
