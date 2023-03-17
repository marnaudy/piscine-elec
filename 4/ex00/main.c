#include <avr/io.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define AHT20_ADDR 0x38

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

int wait_i2c_ready() {
	while (!(TWCR & (1 << TWINT))) {}
	return (TWSR & TW_STATUS_MASK);
}

void i2c_init() {
	//Enable TWI module
	TWCR |= (1 << TWEN);
	//Set baudrate (with prescaler set to 1x)
	TWBR = (uint8_t) (F_CPU / TWI_BAUDRATE / 2 - 8);
}

void i2c_start() {
	//Start TWI transmission
	TWCR |= (1 << TWSTA);
	//Wait for TWI module to be ready to continue
	int status = wait_i2c_ready();
	if (status != TW_START) {
		uart_printstr("I2C has failed to start\r\n");
	}
	//Load AHT20 sensor address and write bit
	TWDR = AHT20_ADDR << 1 | TW_WRITE;
	//Clear start bit
	TWCR &= ~(1 << TWSTA);
	//Clear interruption to send message
	TWCR |= (1 << TWINT);
	//Wait for TWI module to be ready to continue
	status = wait_i2c_ready();
	if (status == TW_MT_SLA_NACK) {
		uart_printstr("No acknowledgement of address packet\r\n");
	}
}

void i2c_stop() {
	//Make sure start bit is clear
	TWCR &= ~(1 << TWSTA);
	//Set stop bit
	TWCR |= (1 << TWSTO);
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
}

void send_status() {
	char *base = "0123456789ABCDEF";
	int status = TW_STATUS;
	uart_printstr("0x");
	uart_tx(base[status / 16]);
	uart_tx(base[status % 16]);
	uart_printstr("\r\n");
}

int main() {
	uart_init();
	i2c_init();
	i2c_start();
	send_status();
	i2c_stop();
	while (1) {}
}
