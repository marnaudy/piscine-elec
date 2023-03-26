#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define IO_EXP_ADDR 0b01000000

enum mode_e {
	potentiometer,
	photoresistor,
	thermistor,
	temp_int,
	forty_two,
	rainbow,
	temp_c,
	temp_f,
	humidity,
	time,
	date,
	year
};

volatile enum mode_e mode = potentiometer;
volatile char display_str[5] = {'8', '8', '8', '8', '\0'};
uint8_t display_position = 0;
volatile _Bool sw1_pressed = 0;
volatile _Bool sw2_pressed = 0;

//------------------------- UART utils -------------------------

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

//------------------------- I2C utils -------------------------

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

//------------------------- ADC utils -------------------------

void adc_init() {
	//Set Vref to AVcc
	ADMUX |= (1 << REFS0);
	//Select RV1 (ADC0) (default
	//ADC clock should be between 50kHz and 200kHz
	//Set prescaler to 128 -> clock = 125kHz
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1)| (1 << ADPS0);
	//Enable ADC
	ADCSRA |= (1 << ADEN);
}

uint16_t adc_get_conv() {
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) {}
	return (ADC);
}

//------------------------- GPIO -------------------------

void io_init() {
	//Set GPIO LEDs to output
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	DDRD |= (1 << DDD3) | (1 << DDD5) | (1 << DDD6);
	//Enable interrupts on change for SW1 and SW2
	SREG |= (1 << SREG_I);
	EICRA |= (1 << ISC00);
	EIMSK |= (1 << INT0);
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
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

void display_n_led(uint8_t n) {
	//Switch all LEDs off
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	//Deal with third LED not being PORTB3
	if (n & 0b1000) {
		PORTB |= (1 << PB4);
	}
	//Display the nice bits
	PORTB |= n & 0b111;
}

//------------------------- Timers -------------------------

void timers_init() {
	//Timer 0 triggers the character display
	//Set timer 0 to normal mode with 64x prescaler and interrupts
	//This will generate interrupts at intervals of 1.02ms
	SREG |= (1 << SREG_I);
	TIMSK0 |= (1 << TOIE0);
	TCCR0B |= (1 << CS01) | (1 << CS00);
	//Timer 2 triggers the update of the display value
	//Set timer to normal mode with 1024x prescaler and interrupts
	//This will generate interrupts at intervals of 16ms
	TIMSK2 |= (1 << TOIE2);
	TCCR2B |= (1 << CS20) | (1 << CS21) | (1 << CS22);
}

//------------------------- Display utils -------------------------

uint8_t get_segment_digit(char c) {
	switch (c) {
	case '0':
		return (0b00111111);
	case '1':
		return (0b00000110);
	case '2':
		return (0b01011011);
	case '3':
		return (0b01001111);
	case '4':
		return (0b01100110);
	case '5':
		return (0b01101101);
	case '6':
		return (0b01111101);
	case '7':
		return (0b00000111);
	case '8':
		return (0b01111111);
	case '9':
		return (0b01101111);
	case '-':
		return (0b01000000);
	}
	return(0);
}

void uint_display(uint16_t n) {
	for (int i = 3; i >= 0; i--) {
		display_str[i] = n % 10 + '0';
		n /= 10;
	}
}

//------------------------- Mode settings -------------------------

void set_mode_potentiometer() {
	//Select potentiometer in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
}

void set_mode_photoresistor() {
	//Select photoresistor in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
	ADMUX |= (1 << MUX0);
}

void set_mode_thermistor() {
	//Select thermistor in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
	ADMUX |= (1 << MUX1);
}

void set_mode(enum mode_e new_mode) {
	display_n_led(new_mode);
	switch (new_mode) {
	case potentiometer:
		set_mode_potentiometer();
		break;
	case photoresistor:
		set_mode_photoresistor();
		break;
	case thermistor:
		set_mode_thermistor();
		break;
	default:
		uint_display(new_mode);
		break;
	}
	mode = new_mode;
}

//------------------------- Update display value -------------------------

void update_value_adc() {
	uint_display(adc_get_conv());
}

//------------------------- Interrupts -------------------------

ISR(TIMER0_OVF_vect) {
	//Display current character on 7 segment display
	//We first need to wipe the display to prevent ghosting
	char c = display_str[display_position];
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
	i2c_write((uint8_t) ~(1 << (4 + display_position)));
	//Set IO1 to display digit
	i2c_write(get_segment_digit(c));
	i2c_stop();
	display_position++;
	if (display_position == 4)
		display_position = 0;
}

ISR(TIMER2_OVF_vect) {
	switch (mode) {
	case potentiometer:
	case photoresistor:
	case thermistor:
		update_value_adc();
		break;
	}
}

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed) {
		enum mode_e new_mode;
		if (mode == year)
			new_mode = 0;
		else
			new_mode = mode + 1;
		set_mode(new_mode);
	}
	_delay_ms(5);
	EIFR |= (1 << INTF0);
}

ISR(PCINT2_vect) {
	sw2_pressed = !sw2_pressed;
	if (sw2_pressed) {
		enum mode_e new_mode;
		if (mode == 0)
			new_mode = year;
		else
			new_mode = mode - 1;
		set_mode(new_mode);
	}
	_delay_ms(5);
	PCIFR |= (1 << PCIF2);
}

ISR(BADISR_vect) {
	uart_print_nl("Bad ISR vector");
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	timers_init();
	adc_init();
	set_mode(potentiometer);
	while (1) {}
}
