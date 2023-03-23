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
volatile int current_led = 0;
volatile int current_colour = 0;
volatile int sw1_pressed;
volatile int sw2_pressed;

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

void switch_init() {
	SREG |= (1 << SREG_I);
	//Generate interrupt on logical change of INT0
	EICRA |= (1 << ISC00);
	//Enable INT0 interrupt
	EIMSK |= (1 << INT0);
	//Enable pin change 2 interrupt
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
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

void update_colour(uint8_t n) {
	switch (current_colour) {
	case 0:
		leds[current_led].r = n;
		break;
	case 1:
		leds[current_led].g = n;
		break;
	case 2:
		leds[current_led].b = n;
		break;
	}
}

ISR(ADC_vect) {
	//Get measurement
	uint8_t pot = ADCH;
	//Print measurement
	uart_print_hex(pot);
	uart_print_nl("");
	update_colour(pot);
	update_rgb_spi();
	//Clear timer1 interrupt flag
	TIFR1 |= (1 << ICF1);
}

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed) {
		current_colour++;
		if (current_colour == 3)
			current_colour = 0;
	}
	_delay_ms(10);
	EIFR &= (1 << INTF0);
}

ISR(PCINT2_vect) {
	sw2_pressed = !sw2_pressed;
	if (sw2_pressed) {
		current_led++;
		if (current_led == 3)
			current_led = 0;
	}
	_delay_ms(10);
	PCIFR &= (1 << PCIF2);
}

int main() {
	for (int i = 0; i < 3; i++) {
		leds[i].brightness = 1;
	}
	uart_init();
	spi_master_init();
	adc_init();
	timer_init();
	switch_init();
	while (1) {}
}
