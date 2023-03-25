#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define IO_EXP_ADDR 0b01000000

volatile uint16_t display_n = 0;
volatile uint8_t digit_position = 0;

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

void adc_init() {
	//Set Vref to AVcc
	ADMUX |= (1 << REFS0);
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

void display_timer_init() {
	//Set timer 0 to normal mode with 64x prescaler and interrupts
	//This will generate interrupts at intervals of 1.02ms
	SREG |= (1 << SREG_I);
	TIMSK0 |= (1 << TOIE0);
	TCCR0B |= (1 << CS01) | (1 << CS00);
}

void adc_timer_init() {
	//Set timer 1 to CTC mode with OCR1A as top and 256x prescaler
	//This will trigger ADC at intervals of 100ms
	TCCR1B |= (1 << WGM12) | (1 << CS12);
	OCR1A = 6250;
	OCR1B = 6250;
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

ISR(TIMER0_OVF_vect) {
	//Display current digit on 7 segment display
	//We first need to wipe the display to prevent ghosting
	uint16_t digit = display_n;
	for (int i = 0; i < digit_position; i++) {
		digit /= 10;
	}
	i2c_start();
	//Address io expander
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Send output port 0 command and wipe
	i2c_write(0x02);
	i2c_write(255);
	i2c_write(0);
	wait_i2c_ready();
	i2c_start();
	i2c_write(IO_EXP_ADDR | TW_WRITE);
	//Restart and display current digit
	i2c_write(0x02);
	//Set IO0 low on current digit CC
	i2c_write((uint8_t) ~(1 << (7 - digit_position)));
	//Set IO1 to display digit
	i2c_write(get_segment_digit(digit % 10));
	i2c_stop();
	digit_position++;
	if (digit_position == 4)
		digit_position = 0;
}

ISR(ADC_vect) {
	//Update display value
	display_n = ADC;
	//Clear timer1 interrupt flag
	TIFR1 |= (1 << OCF1B);
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	display_timer_init();
	adc_init();
	adc_timer_init();
	while (1) {}
}
