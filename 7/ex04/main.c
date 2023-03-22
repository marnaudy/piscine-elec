#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

void uart_init() {
	//Enable transmitter and receiver on USART0
	UCSR0B |= (1 << TXEN0) | (1 << RXEN0);
	//Keep defaults : async with no parity and 1 stop bit
	//Set character size to 8 bits
	UCSR0C |= (1 << UCSZ00) | (1 << UCSZ00);
	//Set baud rate to UART_BAUDRATE
	UBRR0 = (F_CPU / 8 / UART_BAUDRATE - 1) / 2;
}

void uart_tx(char c) {
	//Wait for USART to be ready to transmit
	while (!(UCSR0A & (1 << UDRE0))) {}
	//Transmit character
	UDR0 = c;
}

char uart_rx() {
	//Wait for USART0 to have received something
	while (!(UCSR0A & (1 << RXC0))) {}
	//Return received character
	return (UDR0);
}

void uart_printstr(const char *str) {
	for (int i = 0; str[i]; i++) {
		uart_tx(str[i]);
	}
}

void uart_print_hex(uint8_t n) {
	char *base = "0123456789ABCDEF";
	uart_tx(base[n / 16]);
	uart_tx(base[n % 16]);
}

void uart_print_dec(uint16_t n) {
	char output[6];
	output[5] = 0;
	int i = 4;
	while (i >= 0) {
		output[i] = n % 10 + '0';
		n /= 10;
		if (n == 0)
			break;
		i--;
	}
	uart_printstr(&output[i]);
}

void uart_print_nl(const char *str) {
	uart_printstr(str);
	uart_printstr("\r\n");
}

void adc_init() {
	//Set Vref to AVcc
	ADMUX |= (1 << REFS0);
	//Set left align so all 8 bit are in high byte
	ADMUX |= (1 << ADLAR);
	//Select RV1 (ADC0) (default)
	//Enable auto trigger
	ADCSRA |= (1 << ADATE);
	//Enable interrupts
	SREG |= (1 << SREG_I);
	ADCSRA |= (1 << ADIE);
	//ADC clock should be between 50kHz and 200kHz
	//Set prescaler to 128 -> clock = 125kHz
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1)| (1 << ADPS0);
	//Set trigger source to timer 1 capture event
	ADCSRB |= (1 << ADTS2) | (1 << ADTS1) | (1 << ADTS0);
	//Enable ADC
	ADCSRA |= (1 << ADEN);
}

void timer_init() {
	//Set CTC mode with ICR as top
	TCCR1B |= (1 << WGM12) | (1 << WGM13);
	//Set prescaler to 256x (62.5kHZ)
	TCCR1B |= (1 << CS12);
	//Set ICR (used as top) -> match every 20ms (50Hz = 62.5Hz / 1250)
	ICR1 = 1250;
}

void rgb_init() {
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

void display_gauge(uint8_t n) {
	uint8_t leds = PORTB;
	leds &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	if (n >= 64) {
		leds |= (1 << PB0);
		if (n >= 128) {
			leds |= (1 << PB1);
			if (n >= 192) {
				leds |= (1 << PB2);
				if (n == 255) {
					leds |= (1 << PB4);
				}
			}
		}
	}
	PORTB = leds;
}

ISR(ADC_vect) {
	//Get measurement
	uint8_t pot = ADCH;
	//Print measurement
	uart_print_hex(pot);
	uart_print_nl("");
	wheel(pot);
	display_gauge(pot);
	//Clear timer1 interrupt flag
	TIFR1 |= (1 << ICF1);
}

int main() {
	//Set LEDs 1, 2, 3, and 4 to output
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	rgb_init();
	uart_init();
	adc_init();
	timer_init();
	while (1) {}
}
