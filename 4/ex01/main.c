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

void print_hex_value(unsigned char data) {
	char *base = "0123456789ABCDEF";
	uart_tx(base[data / 16]);
	uart_tx(base[data % 16]);
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

void i2c_start(int read) {
	//Start TWI transmission
	TWCR |= (1 << TWSTA);
	//Wait for TWI module to be ready to continue
	int status = wait_i2c_ready();
	if (status != TW_START) {
		uart_printstr("I2C has failed to start\r\n");
	}
	//Load AHT20 sensor address and write bit
	if (read) {
		TWDR = AHT20_ADDR << 1 | TW_READ;
		//Enable acknowledgement
		TWCR |= (1 << TWEA);
	} else {
		TWDR = AHT20_ADDR << 1 | TW_WRITE;
		//Disable acknowledgement
		TWCR &= ~(1 << TWEA);
	}
	//Clear start bit
	TWCR &= ~(1 << TWSTA);
	//Clear interruption to send message
	TWCR |= (1 << TWINT);
	//Wait for TWI module to be ready to continue
	status = wait_i2c_ready();
	if ((!read && status != TW_MT_SLA_ACK) || (read && status != TW_MR_SLA_ACK)) {
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

void i2c_write(unsigned char data) {
	//Load data
	TWDR = data;
	//Clear start and stop flags
	TWCR &= ~((1 << TWSTA) | (1 << TWSTO));
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
	//Wait for TWI module to send message
	int status = wait_i2c_ready();
	if (status != TW_MT_DATA_ACK) {
		uart_printstr("No acknowledgement of data packet\r\n");
	}
}

unsigned char i2c_read(int end) {
	unsigned char data = TWDR;
	int status = TW_STATUS;
	if (status != TW_MR_DATA_ACK) {
		uart_printstr("Error receiving data\r\n");
	}
	//Clear start and stop flags
	TWCR &= ~((1 << TWSTA) | (1 << TWSTO));
	if (end) {
		//Disable acknowledge flag
		TWCR &= (1 << TWEA);
	}
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
	//Wait for TWI module to read next byte
	wait_i2c_ready();
	return (data);
}

int aht_is_calibrated() {
	i2c_start(1);
	unsigned int data = i2c_read();
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
}

int main() {
	uart_init();
	i2c_init();
	while (1) {}
}
