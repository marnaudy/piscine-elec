#include <avr/io.h>

int main() {
	//Set B1 to output
	DDRB |= (1 << DDB1);
	//Set timer to 0
	TCNT1 = 0;
	//Set timer to CTC mode
	TCCR1B |= (1 << WGM12);
	//The clock will be set at 62500Hz, we want to toggle every half second
	OCR1A = 31250;
	//Set timer 1 to toggle OC1A (AKA B1) on match
	TCCR1A |= (1 << COM1A0);
	//Set timer 1 clock with 256 prescaler
	TCCR1B |= ((1 << CS12));
	while (1) {}
}
