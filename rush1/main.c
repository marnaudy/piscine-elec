#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul
#define IO_EXP_ADDR 0b01000000
#define AHT_ADDR 0b01110000

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
	year,
	start
};

volatile enum mode_e mode = start;
volatile char display_str[5] = {'8', '8', '8', '8', '\0'};
volatile uint8_t decimal_mask = 0b1111;
uint8_t display_position = 0;
volatile _Bool sw1_pressed = 0;
volatile _Bool sw2_pressed = 0;
volatile char colour = 'R';
volatile uint8_t rgb_position = 0;
uint8_t value_refresh_counter = 0;

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

void uart_print_bin(uint8_t n) {
	uart_printstr("0b");
	for (int i = 7; i >= 0; i--) {
		uart_tx(((n >> i) & 1) + '0');
	}
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

//------------------------- SPI utils -------------------------

void spi_master_init() {
	//Set SCK and MOSI and SS to outputs
	DDRB |= (1 << DDB2) | (1 << DDB3) | (1 << DDB5);
	//Set SPI to master
	SPCR |= (1 << MSTR);
	//Set clock divider to 16 -> 1Mhz (acceptable range for LEDs 0.8-1.2MHz)
	SPCR |= (1 << SPR0);
	//Enable SPI
	SPCR |= (1 << SPE);
}

void spi_enable() {
	SPCR |= (1 << SPE);
}

void spi_disable() {
	SPCR &= ~(1 << SPE);
}

void spi_transmit(uint8_t data) {
	SPDR = data;
	//Wait for transmission to finish
	while (!(SPSR & (1 << SPIF))) {}
}

void set_spi_rgb(uint8_t r, uint8_t g, uint8_t b) {
	//Max is 31
	uint8_t brightness = 1;
	//Transmit four bytes of 0s
	for (int i = 0; i < 4; i++) {
		spi_transmit(0);
	}
	//Transmit four LED frames
	for (int i = 0; i < 3; i++) {
		spi_transmit(0b11100000 | brightness);
		spi_transmit(b);
		spi_transmit(g);
		spi_transmit(r);
	}
	//Transmit four bytes of 1s
	for (int i = 0; i < 4; i++) {
		spi_transmit(255);
	}
}

void wheel_spi(uint8_t pos) {
	pos = 255 - pos;
	if (pos < 85) {
		set_spi_rgb(255 - pos * 3, 0, pos * 3);
	} else if (pos < 170) {
		pos = pos - 85;
		set_spi_rgb(0, pos * 3, 255 - pos * 3);
	} else {
		pos = pos - 170;
		set_spi_rgb(pos * 3, 255 - pos * 3, 0);
	}
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

_Bool is_sw3_pressed() {
	_Bool is_pressed;
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
	is_pressed = !(input0 & 1);
	i2c_nack();
	i2c_read();
	//Set stop bit
	TWCR |= (1 << TWSTO) | (1 << TWINT);
	return (is_pressed);
}

void set_all_rgb(char c) {
	uint8_t port_d_save = PORTD;
	port_d_save &= ~((1 << PD3) | (1 << PD5) | (1 << PD6));
	switch (c) {
	case 'R':
		set_spi_rgb(255, 0, 0);
		PORTD = port_d_save | (1 << PD5);
		break;
	case 'G':
		set_spi_rgb(0, 255, 0);
		PORTD = port_d_save | (1 << PD6);
		break;
	case 'B':
		set_spi_rgb(0, 0, 255);
		PORTD = port_d_save | (1 << PD3);
		break;
	default:
		set_spi_rgb(0, 0, 0);
		PORTD = port_d_save;
		break;
	}
}

//------------------------- Timers -------------------------

void timers_init() {
	//Timer 0 triggers the character display
	//Set timer 0 to normal mode with 64x prescaler and interrupts
	//This will generate interrupts at intervals of 1.02ms
	SREG |= (1 << SREG_I);
	TIMSK0 |= (1 << TOIE0);
	TCCR0B |= (1 << CS01) | (1 << CS00);
	//Timer 1 will be used to control RGB effects
	//Set to CTC mode with OCR1A as top and 1024x prescaler
	//Interrupts will be off for now
	TCCR1B |= (1 << WGM12) | (1 << CS10) | (1 << CS12);
	//Timer 2 triggers the update of the display value
	//Set timer to normal mode with 1024x prescaler and no interrupts
	//This will be used to generate interrupts at intervals of 16ms
	TCCR2B |= (1 << CS20) | (1 << CS21) | (1 << CS22);
}

void start_value_update_timer() {
	TIMSK2 |= (1 << TOIE2);
}

//------------------------- Display utils -------------------------

uint8_t get_segment_char(char c) {
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
	case 'C':
		return (0b00111001);
	case 'F':
		return (0b01110001);
	case 'H':
		return (0b01110110);
	}
	return(0);
}

void uint_display(uint16_t n) {
	for (int i = 3; i >= 0; i--) {
		display_str[i] = n % 10 + '0';
		n /= 10;
	}
}

void float_display(float f) {
	_Bool is_negative = 0;
	if (f < 0) {
		is_negative = 1;
		f = -f;
	}
	if ((int) (f / 100))
		display_str[0] = (int) (f / 100) % 10 + '0';
	else
		display_str[0] = ' ';
	if ((int) (f / 10))
		display_str[1] = (int) (f / 10) % 10 + '0';
	else
		display_str[0] = ' ';
	display_str[2] = (int) f % 10 + '0';
	display_str[3] = (int) (f * 10) % 10 + '0';
	if (is_negative)
		display_str[0] = '-';
}

//------------------------- AHT utils -------------------------

void aht_request_measurement() {
	//Send measurement command
	i2c_start();
	i2c_write(AHT_ADDR | TW_WRITE);
	i2c_write(0xAC);
	i2c_write(0x33);
	i2c_write(0);
	i2c_stop();
	//Generate interrupt in 80ms when measurement is ready
	OCR1A = 1250;
	TIFR1 |= (1 << OCF1A);
	TIMSK1 |= (1 << OCIE1A);
	TCNT1 = 0;
}

uint64_t aht_get_value() {
	//Disable interrupts on timer1
	TIMSK1 &= ~(1 << OCIE1A);
	uint64_t res = 0;
	i2c_start();
	i2c_write(AHT_ADDR | TW_READ);
	wait_i2c_ready();
	i2c_ack();
	uint8_t status = i2c_read();
	if ((status & (1 << 7))) {
		uart_print_nl("AHT didn't wait long enough");
		i2c_nack();
		i2c_stop();
		return (0);
	}
	if (!(status & (1 << 3)))
		uart_print_nl("AHT not calibrated");
	for (int i = 4; i >= 0; i--) {
		if (i != 0)
			i2c_ack();
		else
			i2c_nack();
		uint8_t d = i2c_read();
		res = res << 8;
		res |= d;
	}
	i2c_stop();
	return (res);
}

float aht_get_temp_c() {
	float res;
	uint64_t data = aht_get_value();
	data &= 0xFFFFF;
	res = ((float) data) / 0x100000;
	return (res * 200 - 50);
}

float aht_get_temp_f() {
	return (aht_get_temp_c() * 9 / 5 + 32);
}

float aht_get_humidity() {
	float res;
	uint64_t data = aht_get_value();
	data = data >> 20;
	res = ((float) data) / 0x100000;
	return (res * 100);
}

//------------------------- Mode settings -------------------------

void set_mode_potentiometer() {
	decimal_mask = 0;
	//Select AVcc as Vref
	ADMUX &= ~(1 << REFS1);
	//Select potentiometer in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
}

void set_mode_photoresistor() {
	//Select AVcc as Vref
	ADMUX &= ~(1 << REFS1);
	//Select photoresistor in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
	ADMUX |= (1 << MUX0);
}

void set_mode_thermistor() {
	//Select AVcc as Vref
	ADMUX &= ~(1 << REFS1);
	//Select thermistor in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
	ADMUX |= (1 << MUX1);
}

void set_mode_temp_int() {
	//Select 1.1V as Vref
	ADMUX |= (1 << REFS1);
	//Select internal temp in ADC
	ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));
	ADMUX |= (1 << MUX3);
}

void set_mode_forty_two() {
	display_str[0] = '-';
	display_str[1] = '4';
	display_str[2] = '2';
	display_str[3] = '-';
	colour = 'R';
	spi_enable();
	set_all_rgb(colour);
	//Activate timer1 to generate interrupts every second
	OCR1A = 15625;
	TIFR1 |= (1 << OCF1A);
	TIMSK1 |= (1 << OCIE1A);
	TCNT1 = 0;
}

void set_mode_rainbow() {
	display_str[0] = '-';
	display_str[1] = '4';
	display_str[2] = '2';
	display_str[3] = '-';
	rgb_position = 0;
	spi_enable();
	//Activate timer1 to generate interrupts so full cycle lasts ~4s
	OCR1A = 255;
	TIFR1 |= (1 << OCF1A);
	TIMSK1 |= (1 << OCIE1A);
	TCNT1 = 0;
}

void unset_mode_rgb() {
	//Turn off interrupts on timer 1
	TIMSK1 &= ~(1 << OCIE1A);
	set_all_rgb('0');
	spi_disable();
}

void set_decimal_point() {
	decimal_mask = 0b0100;
}

void unset_decimal_point() {
	decimal_mask = 0;
}

void set_mode(enum mode_e new_mode) {
	switch (mode) {
	case forty_two:
	case rainbow:
		unset_mode_rgb();
		break;
	case temp_c:
	case temp_f:
	case humidity:
		unset_decimal_point();
		break;
	}
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
	case temp_int:
		set_mode_temp_int();
		break;
	case forty_two:
		set_mode_forty_two();
		break;
	case rainbow:
		set_mode_rainbow();
		break;
	case temp_c:
	case temp_f:
	case humidity:
		set_decimal_point();
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

void update_value_temp_int() {
	uint16_t adc_value = adc_get_conv();
	uint_display(adc_value - 342);
}

//------------------------- Interrupts -------------------------

ISR(TIMER0_OVF_vect) {
	//Display current character on 7 segment display
	//We first need to wipe the display to prevent ghosting
	char c = display_str[display_position];
	_Bool sw3_pressed = is_sw3_pressed();
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
	//Set IO0 low on current digit CC and LEDs if switches are pressed
	uint8_t io0 = (uint8_t) ~(1 << (4 + display_position));
	if (sw1_pressed)
		io0 &= ~(1 << 3);
	if (sw2_pressed)
		io0 &= ~(1 << 2);
	if (sw3_pressed)
		io0 &= ~(1 << 1);
	i2c_write(io0);
	//Set IO1 to display digit
	if (decimal_mask & (1 << display_position))
		i2c_write(get_segment_char(c) | (1 << 7));
	else
		i2c_write(get_segment_char(c));
	i2c_stop();
	display_position++;
	if (display_position == 4)
		display_position = 0;
}

ISR(TIMER2_OVF_vect) {
	if (value_refresh_counter == 9) {
		switch (mode) {
		case potentiometer:
		case photoresistor:
		case thermistor:
			update_value_adc();
			break;
		case temp_int:
			update_value_temp_int();
			break;
		case temp_c:
		case temp_f:
		case humidity:
			aht_request_measurement();
			break;
		}
		value_refresh_counter = 0;
	} else {
		value_refresh_counter++;
	}
}

ISR(TIMER1_COMPA_vect) {
	if (mode == forty_two) {
		//Select next rgb effect
		if (colour == 'R')
			colour = 'G';
		else if (colour == 'G')
			colour = 'B';
		else if (colour == 'B')
			colour = 'R';
		set_all_rgb(colour);
	} else if (mode == rainbow) {
		wheel_spi(rgb_position);
		rgb_position++;
	} else if (mode == temp_c) {
		float_display(aht_get_temp_c());
		if (display_str[0] == ' ')
			display_str[0] = 'C';
	} else if (mode == temp_f) {
		float_display(aht_get_temp_f());
		if (display_str[0] == ' ')
			display_str[0] = 'F';
	} else if (mode == humidity) {
		float_display(aht_get_humidity());
		if (display_str[0] == ' ')
			display_str[0] = 'H';
	}
}

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed && mode != start) {
		enum mode_e new_mode;
		if (mode == year)
			new_mode = 0;
		else
			new_mode = mode + 1;
		set_mode(new_mode);
	}
	_delay_ms(10);
	EIFR |= (1 << INTF0);
}

ISR(PCINT2_vect) {
	sw2_pressed = !sw2_pressed;
	if (sw2_pressed && mode != start) {
		enum mode_e new_mode;
		if (mode == 0)
			new_mode = year;
		else
			new_mode = mode - 1;
		set_mode(new_mode);
	}
	_delay_ms(10);
	PCIFR |= (1 << PCIF2);
}

ISR(BADISR_vect) {
	uart_print_nl("Bad ISR vector");
}

void start_animation() {
	PORTB |= ((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	//Wait 3s
	OCR1A = 46875;
	TCNT1 = 0;
	TIFR1 |= (1 << OCF1A);
	while (!(TIFR1 & (1 << OCF1A))) {}
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	//Wait 1s
	OCR1A = 15625;
	TCNT1 = 0;
	TIFR1 |= (1 << OCF1A);
	while (!(TIFR1 & (1 << OCF1A))) {}
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	adc_init();
	spi_master_init();
	set_all_rgb(0);
	spi_disable();
	timers_init();
	start_animation();
	set_mode(potentiometer);
	start_value_update_timer();
	while (1) {}
}
