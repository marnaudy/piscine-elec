#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

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

int main() {
	init_rgb();
	while (1) {
		set_rgb(255, 0, 0);
		_delay_ms(1000);
		set_rgb(0, 255, 0);
		_delay_ms(1000);
		set_rgb(0, 0, 255);
		_delay_ms(1000);
		set_rgb(255, 255, 0);
		_delay_ms(1000);
		set_rgb(0, 255, 255);
		_delay_ms(1000);
		set_rgb(255, 0, 255);
		_delay_ms(1000);
		set_rgb(255, 255, 255);
		_delay_ms(1000);
	}
}
