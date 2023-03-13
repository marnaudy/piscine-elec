#include <avr/io.h>

int main() {
	//Set direction of B0 to output
	DDRB |= (1 << DDB0);
	//Set direction of D2 to output
	DDRD &= ~(1 << DDD2);
	while (1) {
		//Check input if D2
		if (PIND & (1 << PD2))
			//Set bit 0 (denoted by PB0) of Port B to 0
			PORTB &= ~(1 << PB0);
		else
			//Set bit 0 (denoted by PB0) of Port B to 1
			PORTB |= (1 << PB0);
	}
}
