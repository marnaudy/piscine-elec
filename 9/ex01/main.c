#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define IO_EXP_ADDR 0b01000000

uint8_t n = 0;
uint8_t sw3_prev_status = 1;

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

void i2c_write(unsigned char data) {
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

void display(uint8_t n) {
	i2c_start();
	//Address io expander
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Send output port 0 command
	i2c_write(0x02);
	//Display n on LEDs
	n = n % 8;
	n = ~(n << 1);
	i2c_write(n);
	//Set IO1 low
	i2c_write(0);
	i2c_stop();
}

void check_input() {
	i2c_start();
	//Address io expander
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Send input0 command
	i2c_write(0);
	wait_i2c_ready();
	i2c_start();
	i2c_write(IO_EXP_ADDR | TW_READ);
	wait_i2c_ready();
	i2c_ack();
	uint8_t input0 = i2c_read();
	input0 = input0 & 1;
	i2c_nack();
	i2c_read();
	if (!input0 && input0 != sw3_prev_status) {
		n++;
		display(n);
	} else {
		//Set stop bit
		TWCR |= (1 << TWSTO) | (1 << TWINT);
	}
	sw3_prev_status = input0;
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	display(n);
	while (1) {
		_delay_ms(1);
		check_input();
	}
}
