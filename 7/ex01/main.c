#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

enum sensors {
	potentiometer,
	photoresistor,
	thermistor
};

volatile enum sensors current_sensor = potentiometer;

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

void uart_print_nl(const char *str) {
	uart_printstr(str);
	uart_printstr("\r\n");
}

void adc_init() {
	//Set Vref to AVcc
	ADMUX |= (1 << REFS0);
	//Left adjust result so first 8 bits are in same byte
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
	//Set trigger source to timer 1 compare B
	ADCSRB |= (1 << ADTS2) | (1 << ADTS0);
	//Enable ADC
	ADCSRA |= (1 << ADEN);
}

void timer_init() {
	//Set CTC mode
	TCCR1B |= (1 << WGM12);
	//Set prescaler to 256x (62.5kHZ)
	TCCR1B |= (1 << CS12);
	//Set A and B -> match every 20ms (50Hz = 62.5Hz / 1250)
	//ADC can only accept match B as trigger, A is set to reset the timer on match
	OCR1A = 1250;
	OCR1B = 1250;
}

ISR(ADC_vect) {
	//Print 8 most significant bits
	uart_print_hex(ADCH);
	//Print separation and select next sensor
	switch (current_sensor) {
	case potentiometer:
		uart_printstr(", ");
		current_sensor = photoresistor;
		ADMUX |= (1 << MUX0);
		//Trigger new conversion
		ADCSRA |= (1 << ADSC);
		break;
	case photoresistor:
		uart_printstr(", ");
		current_sensor = thermistor;
		ADMUX &= ~(1 << MUX0);
		ADMUX |= (1 << MUX1);
		//Trigger new conversion
		ADCSRA |= (1 << ADSC);
		break;
	case thermistor:
		uart_print_nl("");
		current_sensor = potentiometer;
		ADMUX &= ~((1 << MUX0) | (1 << MUX1));
		break;
	}
	//Clear timer1 interrupt flag
	TIFR1 |= (1 << OCF1B);
}

int main() {
	uart_init();
	adc_init();
	timer_init();
	while (1) {}
}
