#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define IO_EXP_ADDR 0b01000000

void uart_init() {
	//Enable transmitter on USART0
	UCSR0B |= (1 << TXEN0);
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

void uart_printstr(const char *str) {
	for (int i = 0; str[i]; i++) {
		uart_tx(str[i]);
	}
}

void uart_print_nl(const char *str) {
	uart_printstr(str);
	uart_printstr("\r\n");
}

void uart_print_hex(uint8_t n) {
	char *base = "0123456789ABCDEF";
	uart_tx(base[n / 16]);
	uart_tx(base[n % 16]);
}

void i2c_init() {
	//Set baudrate (with prescaler set to 1x)
	TWBR = (uint8_t) (F_CPU / TWI_BAUDRATE / 2 - 8);
	//Enable TWI module
	TWCR |= (1 << TWEN);
}

int wait_i2c_ready() {
	while ((TWCR & (1 << TWINT)) == 0) {}
	return (TW_STATUS);
}

void i2c_start() {
	//Set start bit and clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWSTA) | (1 << TWINT);
}

void i2c_stop() {
	wait_i2c_ready();
	//Set stop bit
	TWCR |= (1 << TWSTO);
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
}

void i2c_write(uint8_t data) {
	wait_i2c_ready();
	//Clear start flag while not "clearing" TWINT
	TWCR &= ~((1 << TWSTA) | (1 << TWINT));
	TWDR = data;
	//Clear interrupt
	TWCR |= (1 << TWINT);
}

uint8_t i2c_read() {
	wait_i2c_ready();
	return (TWDR);
}

void i2c_ack() {
	TWCR |= (1 << TWEA) | (1 << TWINT);
}

void i2c_nack() {
	TWCR &= ~((1 << TWEA) | (1 << TWINT));
	TWCR |= (1 << TWINT);
}

void io_init() {
	i2c_start();
	//Address io expander
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Send configuration command
	i2c_write(0x06);
	//Set IO0_0 to input
	i2c_write(1);
	//Set IO1 to output
	i2c_write(0);
	i2c_stop();
}

uint8_t get_segment_digit(uint8_t n) {
	switch (n % 10) {
	case 0:
		return (0b00111111);
	case 1:
		return (0b00000110);
	case 2:
		return (0b01011011);
	case 3:
		return (0b01001111);
	case 4:
		return (0b01100110);
	case 5:
		return (0b01101101);
	case 6:
		return (0b01111101);
	case 7:
		return (0b00000111);
	case 8:
		return (0b01111111);
	case 9:
		return (0b01101111);
	}
}

void display(uint8_t n) {
	i2c_start();
	//Address io expander
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Send output port 0 command
	i2c_write(0x02);
	//Set IO0 low on CC4 (IO0_7)
	i2c_write((uint8_t) ~(1 << 7));
	//Set IO1 to display digit
	i2c_write(get_segment_digit(n));
	i2c_stop();
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	int n = 0;
	while (1) {
		display(n);
		_delay_ms(1000);
		n++;
	}
}
