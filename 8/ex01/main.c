#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

typedef struct led_data_s {
	uint8_t brightness;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} led_setting;

volatile led_setting leds[3];

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

int main() {
	uart_init();
	spi_master_init();
	leds[0].brightness = 1;
	while (1) {
		leds[0].r = 255;
		leds[0].g = 0;
		leds[0].b = 0;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].r = 0;
		leds[0].g = 255;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].g = 0;
		leds[0].b = 255;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].r = 255;
		leds[0].g = 255;
		leds[0].b = 0;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].r = 0;
		leds[0].b = 255;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].r = 255;
		leds[0].g = 0;
		update_rgb_spi();
		_delay_ms(1000);
		leds[0].g = 255;
		update_rgb_spi();
		_delay_ms(1000);
	}
}
