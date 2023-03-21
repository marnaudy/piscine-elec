#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define MAX_INPUT_SIZE 10

uint8_t position = 0;

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

void uart_print_nl(const char *str) {
	uart_printstr(str);
	uart_printstr("\r\n");
}

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

int get_hex(char *input) {
	uart_print_nl("Enter a colour hex value (#RRGGBB):");
	int i = 0;
	while (i < MAX_INPUT_SIZE) {
		input[i] = uart_rx();
		if (input[i] == 127 && i != 0) {
			uart_printstr("\b \b");
			i--;
			continue;
		}
		if (input[i] == '\r') {
			input[i] = 0;
			break;
		}
		if (input[i] >= 20 && input[i] < 127)
			uart_tx(input[i]);
		if (input[i] != 127)
			i++;
	}
	uart_print_nl("");
	if (i == MAX_INPUT_SIZE)
		return (-1);
	return (0);
}

int is_in_set(char *set, char c) {
	for (int i = 0; set[i]; i++) {
		if (set[i] == c)
			return (1);
	}
	return (0);
}

int check_hex(char *input) {
	if (input[0] != '#')
		return (1);
	int i = 1;
	while (input[i]) {
		if (!is_in_set("0123456789ABCDEF", input[i]))
			return (1);
		i++;
	}
	if (i != 7)
		return (1);
	return (0);
}

uint8_t char_hex_to_int(char c) {
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'A' && c <= 'F')
		return (c - 'F' + 10);
	return (0);
}

uint8_t hex_to_int(char *str) {
	if (!str[0] || !str[1])
		return (0);
	return ((char_hex_to_int(str[0]) << 4) | char_hex_to_int(str[1]));
}

void display_hex(char *input) {
	uint8_t r, g, b;
	r = hex_to_int(&input[1]);
	g = hex_to_int(&input[3]);
	b = hex_to_int(&input[5]);
	set_rgb(r, g, b);
}

int main() {
	char input[MAX_INPUT_SIZE];
	uart_init();
	init_rgb();
	while (1) {
		if (get_hex(input)) {
			uart_print_nl("Input is too long");
			uart_print_nl("");
			continue;
		}
		if (check_hex(input)) {
			uart_print_nl("Format is incorrect");
			uart_print_nl("");
			continue;
		}
		display_hex(input);
	}
}
