#include <avr/io.h>

int main() {
	//Set B1 to output
	DDRB |= (1 << DDB1);
	//Set timer to 0
	TCNT1 = 0;
	//Set timer to PWM mode with ICR1 (Input Capture Register) as TOP
	TCCR1B |= (1 << WGM13);
	//The clock will be set at 62500Hz, 1 cycle = 1s so TOP (ICR1) should be half
	ICR1 = 31250;
	//Set Output Compare to 1/10th of TOP 
	OCR1A = 3125;
	//Set timer 1 to non inverted mode (clears OCA when counting up and sets when counting down)
	TCCR1A |= (1 << COM1A1);
	//Set timer 1 clock with 256 prescaler
	TCCR1B |= ((1 << CS12));
	while (1) {}
}
