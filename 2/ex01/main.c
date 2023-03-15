#include <avr/io.h>
#include <avr/interrupt.h>

//Timer 1 will control the brightness of LED1 through the OCR1A / ICR1 ratio
//Timer 0 will modify OCR1A through an interupt 20k times per second

int increasing = 1;

ISR(TIMER0_COMPA_vect) {
	if (increasing) {
		OCR1A++;
		if (OCR1A == ICR1) {
			increasing = 0;
		}
	} else {
		OCR1A--;
		if (OCR1A == 0) {
			increasing = 1;
		}
	}
}

ISR(BADISR_vect) {
	//Light LED2 to signal unhandled interrupt
	PORTB |= (1 << PB2);
}

int main() {
	//Set B1 to output
	DDRB |= (1 << DDB1) | (1 << DDB2);
	//Enable interrupts
	SREG |= (1 << SREG_I);
	//Set timers 0 and 1 to 0
	TCNT0 = 0;
	TCNT1 = 0;
	//Set timer 1 to PWM mode with ICR1 (Input Capture Register) used as TOP (max value for counter)
	TCCR1B |= (1 << WGM13);
	//Set timer 1 to non inverted mode (clears OCA when counting up and sets when counting down)
	TCCR1A |= (1 << COM1A1);
	//The ratio OCR1A / ICR1 will determine the duty cycle of OC1A (and brightness of LED)
	ICR1 = 1000;
	//Enable timer 1 clock at full speed
	TCCR1B |= ((1 << CS10));
	//Set timer 0 to count mode
	TCCR0A |= (1 << WGM01);
	//Set timer 0 to loop and send interrupt every 125 ticks (2kHz)
	OCR0A = 125;
	//Enable interupt on compare match with OCR0A
	TIMSK0 |= (1 << OCIE0A);
	//Set clock with 64x prescaler (250kHz)
	TCCR0B |= (1 << CS01) | ( 1 << CS00);
	while (1) {}
}
