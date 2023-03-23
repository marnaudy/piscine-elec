#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define MAX_INPUT_SIZE 15

typedef struct led_data_s {
	uint8_t brightness;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} led_setting;

volatile led_setting leds[3];
volatile uint8_t position = 0;

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

void wheel_init() {
	//Set timer 1 to 0
	TCNT1 = 0;
	//Set timer 1 to CTC mode
	TCCR1B |= (1 << WGM12);
	//Set interrupt generation frequency. One cycle will last OCR1A*255*PRESCALER/F_CPU
	//With OCR1A = 255 the animation will last ~4s
	OCR1A = 255;
	//Enable interrupts globally
	//Don't enable interrupt on OCR1A match yet (this will enable rainbow mode)
	SREG |= (1 << SREG_I);
	//Enable timer 1 with 1024 prescaler
	TCCR1B |= (1 << CS12) | (1 << CS10);
}

void spi_master_init() {
	//Set SCK, MOSI and SS to outputs
	DDRB |= (1 << DDB2) | (1 << DDB3) | (1 << DDB5);
	// DDRB |= (1 << DDB3) | (1 << DDB5);
	//Set SPI to master
	SPCR |= (1 << MSTR);
	//Set clock divider to 16 -> 1Mhz (acceptable range for LEDs 0.8-1.2MHz)
	SPCR |= (1 << SPR0);
	//Enable SPI
	SPCR |= (1 << SPE);
}

void spi_transmit(uint8_t data) {
	SPDR = data;
	//Wait for transmission to finish
	while (!(SPSR & (1 << SPIF))) {}
}

void update_rgb_spi() {
	//Transmit four bytes of 0s
	for (int i = 0; i < 4; i++) {
		spi_transmit(0);
	}
	//Transmit four LED frames
	for (int i = 0; i < 3; i++) {
		spi_transmit(0b11100000 | leds[i].brightness);
		spi_transmit(leds[i].b);
		spi_transmit(leds[i].g);
		spi_transmit(leds[i].r);
	}
	//Transmit four bytes of 1s
	for (int i = 0; i < 4; i++) {
		spi_transmit(255);
	}
}

void set_rgb_one(uint8_t r, uint8_t g, uint8_t b, uint8_t led_n) {
	leds[led_n].r = r;
	leds[led_n].g = g;
	leds[led_n].b = b;
}

void set_rgb_all(uint8_t r, uint8_t g, uint8_t b) {
	for (int i = 0; i < 3; i++) {
		leds[i].r = r;
		leds[i].g = g;
		leds[i].b = b;
	}
}

void wheel(uint8_t pos, uint8_t led_n) {
	pos = 255 - pos;
	if (pos < 85) {
		set_rgb_one(255 - pos * 3, 0, pos * 3, led_n);
	} else if (pos < 170) {
		pos = pos - 85;
		set_rgb_one(0, pos * 3, 255 - pos * 3, led_n);
	} else {
		pos = pos - 170;
		set_rgb_one(pos * 3, 255 - pos * 3, 0, led_n);
	}
}

int get_hex(char *input) {
	uart_print_nl("Enter a colour hex value (#RRGGBBDX):");
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

int str_match(char *str1, char *str2) {
	int i = 0;
	while (str1[i] && str1[i] == str2[i]) {
		i++;
	}
	return (str1[i] == str2[i]);
}

int check_hex(char *input) {
	if (str_match(input, "#FULLRAINBOW"))
		return (0);
	if (input[0] != '#')
		return (1);
	int i = 1;
	while (input[i] && i < 7) {
		if (!is_in_set("0123456789ABCDEF", input[i]))
			return (1);
		i++;
	}
	if (i != 7 || input[i] != 'D' || !is_in_set("678", input[i + 1]))
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
	if (str_match(input, "#FULLRAINBOW")) {
		TIMSK1 |= (1 << OCIE1A);
		uart_print_nl("Activating rainbow");
	} else {
		//Disable rainbow mode
		TIMSK1 &= ~(1 << OCIE1A);
		int led_n = input[8] - '6';
		leds[led_n].r = hex_to_int(&input[1]);
		leds[led_n].g = hex_to_int(&input[3]);
		leds[led_n].b = hex_to_int(&input[5]);
		update_rgb_spi();
	}
}

ISR(TIMER1_COMPA_vect) {
	uint8_t offset = 25;
	wheel(position, 0);
	wheel(position - offset, 1);
	wheel(position - offset, 2);
	position++;
	update_rgb_spi();
}


int main() {
	char input[MAX_INPUT_SIZE];
	for (int i = 0; i < 3; i++) {
		leds[i].brightness = 1;
	}
	uart_init();
	spi_master_init();
	wheel_init();
	update_rgb_spi();
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
