#include <avr/io.h>
#include <util/delay.h>

int main() {
	//Set direction of B0 to output
	DDRB |= (1 << DDB0);
	//Set direction of D2 to output
	DDRD &= ~(1 << DDD2);
	while (1) {
		//Check input if D2
		if (!(PIND & (1 << PD2))) {
			//Flip bit 0 (denoted by PB0) of Port B
			PORTB ^= (1 << PB0);
			//Wait 20ms to avoid bounce on push
			_delay_ms(20);
			//Wait until button is released
			while (!(PIND & (1 << PD2))) {}
			//Wait 20ms to avoid bounce on release
			_delay_ms(20);
		}
	}
}
