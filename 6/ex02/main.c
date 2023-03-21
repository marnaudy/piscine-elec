#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

uint8_t position = 0;

void init_rgb() {
	//Set RGB LED to output
	DDRD |= (1 << DDD3) | (1 << DDD5) | (1 << DDD6);
	//Set timer 0 and 2 to 0
	TCNT0 = 0;
	TCNT2 = 0;
	//Set timers to PWM phase correct mode
	TCCR0A |= (1 << WGM00);
	TCCR2A |= (1 << WGM20);
	//Set Output Compare to 0 to initialise LED off
	OCR0A = 0;
	OCR0B = 0;
	OCR2B = 0;
	//Set timers to non inverted mode switches LEDs off when counting up and on when down
	TCCR0A |= (1 << COM0A1) | (1 << COM0B1);
	TCCR2A |= (1 << COM2B1);
	//Set timer clocks with no prescaler
	TCCR0B |= ((1 << CS00));
	TCCR2B |= ((1 << CS20));
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
	OCR0B = r;
	OCR0A = g;
	OCR2B = b;
}

void wheel(uint8_t pos) {
	pos = 255 - pos;
	if (pos < 85) {
		set_rgb(255 - pos * 3, 0, pos * 3);
	} else if (pos < 170) {
		pos = pos - 85;
		set_rgb(0, pos * 3, 255 - pos * 3);
	} else {
		pos = pos - 170;
		set_rgb(pos * 3, 255 - pos * 3, 0);
	}
}

void init_wheel() {
	//Set timer 1 to 0
	TCNT1 = 0;
	//Set timer 1 to CTC mode
	TCCR1B |= (1 << WGM12);
	//Set interrupt generation frequency. One cycle will last OCR1A*255*PRESCALER/F_CPU
	//With OCR1A = 255 the animation will last ~4s
	OCR1A = 255;
	//Enable interrupt on OCR1A match
	SREG |= (1 << SREG_I);
	TIMSK1 |= (1 << OCIE1A);
	//Enable timer 1 with 1024 prescaler
	TCCR1B |= (1 << CS12) | (1 << CS10);
}

ISR(TIMER1_COMPA_vect) {
	wheel(position);
	position++;
}

int main() {
	init_rgb();
	init_wheel();
	while (1) {}
}
