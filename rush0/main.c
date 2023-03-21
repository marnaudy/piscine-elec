#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>

#define TWI_BAUDRATE 100000ul
#define ERROR 0xff
#define LOSE 0xfe
#define WIN 0xfd
// #define ISR(vector, ...)\
// 	void vector (void) __attribute__ ((signal,__INTR_ATTRS)) __VA_ARGS__; \
// 	void vector (void)
#include <avr/interrupt.h>

enum g_stat {
	lobby,
	ready,
	countdown,
	playing,
	lose,
	win,
	r_error,
	s_error
};

volatile int nb_players = 3;
volatile int nb_ready = 0;
volatile enum g_stat game_status = lobby;
volatile _Bool twi_busy = 0;
_Bool sw1_pressed = 0;
_Bool sw2_pressed = 0;

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

void i2c_init() {
	//Enable TWI module and acknowledgement
	TWCR |= (1 << TWEN) | (1 << TWEA);
	//Enable general call recognition
	TWAR |= (1 << TWGCE);
	//Set baudrate (with prescaler set to 1x)
	TWBR = (uint8_t) (F_CPU / TWI_BAUDRATE / 2 - 8);
}

void i2c_start() {
	//Set start bit and clear TWINT bit (notifies module to continue)
	// TWCR |= (1 << TWSTA) | (1 << TWINT);
	TWCR |= (1 << TWSTA);
}

void i2c_stop() {
	//Make sure start bit is clear
	// TWCR &= ~(1 << TWSTA);
	//Set stop bit
	TWCR |= (1 << TWSTO);
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
	twi_busy = 0;
}

void i2c_write(unsigned char data) {
	TWDR = data;
	//Clear start flag
	TWCR &= ~(1 << TWSTA);
	//Clear interrupt
	TWCR |= (1 << TWINT);
}

void set_rgb(char colour) {
	//Reset switch all rgb off
	PORTD &= ~((1 << PD3) | (1 << PD5) | (1 << PD6));
	if (colour == 'R') {
		PORTD |= (1 << PD5);
	}
	if (colour == 'G') {
		PORTD |= (1 << PD6);
	}
	if (colour == 'B') {
		PORTD |= (1 << PD3);
	}
}

void io_init() {
	//Set LEDs to output
	DDRB = (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	DDRD = (1 << DDD3) | (1 << DDD5) | (1 << DDD6);
	//Switch LED to red for not ready state
	set_rgb('R');
}

void display(int n) {
	//Switch all LEDs off
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	//Deal with third LED not being PORTB3
	if (n & 0b1000) {
		PORTB |= (1 << PB4);
	}
	//Display the nice bits
	PORTB |= n & 0b111;
}

void io_countdown(void) {
	set_rgb('B');
	int n = 15;
	int i = 8;
	while (i > 0 && game_status == countdown) {
		display(n);
		_delay_ms(500);
		n -= i;
		i /= 2;
	}
	display(n);
	if (game_status != countdown)
		return;
	_delay_ms(500);
	PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);
}

void io_win(void) {
	//Set led GREEN
	set_rgb('G');
	//Knight RIDERR
	unsigned int n = 1;
	int direction = 1;
	uart_print_nl("Bravoooo !");
	for (int i = 0; i < 20; i++)
	{
		display(n);
		if (direction)
			n *= 2;
		else
			n /= 2;
		if (n == 1)
			direction = 1;
		if (n == 8)
			direction = 0;
		_delay_ms(150);
	}
}

void io_lose(void) {
	uart_print_nl("Soit meilleur...");
	set_rgb('I');
	//Set leds to red
	for (int i = 0; i < 6; i++)
	{
		PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);
		_delay_ms(250);
		PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
		_delay_ms(250);

	}
}

void io_error(void) {
	for (int i = 0; i < 3; i++)
	{
		set_rgb('R');
		_delay_ms(150);
		set_rgb('I');
		_delay_ms(150);
	}
}

void interrupt_init() {
	//Enable interrupts globally
	SREG |= (1 << SREG_I);
	//Enable interrupts on SW1 (INT0)
	EIMSK |= (1 << INT0);
	EICRA |= (1 << ISC00);
	//Enable interrupts on SW2 (PCINT20)
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
	//Enable interrupts on TWI
	TWCR |= (1 << TWIE);
}

void print_twi_status() {
	char *base = "0123456789ABCDEF";
	int status = TW_STATUS;
	uart_printstr("0x");
	uart_tx(base[status / 16]);
	uart_tx(base[status % 16]);
	uart_printstr("\r\n");
}

void print_game_status() {
	switch(game_status) {
		case lobby:
			uart_print_nl("Status = lobby");
			break;
		case ready:
			uart_print_nl("Status = ready");
			break;
		case countdown:
			uart_print_nl("Status = countdown");
			break;
		case playing:
			uart_print_nl("Status = playing");
			break;
		case win:
			uart_print_nl("Status = win");
			break;
		case lose:
			uart_print_nl("Status = lose");
			break;
		case s_error:
			uart_print_nl("Status = s_error");
			break;
		case r_error:
			uart_print_nl("Status = r_error");
			break;
	}
}

void set_mode_lobby() {
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	set_rgb('R');
	nb_ready = 0;
	game_status = lobby;
}

void set_mode_ready() {
	set_rgb('G');
	game_status = ready;
}

void set_mode_countdown() {
	game_status = countdown;
}

void set_mode_playing() {
	game_status = playing;
}

void set_mode_win() {
	game_status = win;
}

void set_mode_lose() {
	game_status = lose;
}

void set_mode_s_error() {
	game_status = s_error;
	if (!twi_busy) {
		TWCR |= (1 << TWSTA);
	}
}

void set_mode_r_error() {
	game_status = r_error;
}

void n_ready(unsigned char c) {
	if (c != nb_ready && c != ERROR)
	{
		uart_print_nl("Error nb players");
		set_mode_s_error();
	}
	else if (c == ERROR)
		set_mode_r_error();
	nb_ready++;
}

void p_lose(unsigned char c) {
	if (c != LOSE && c != ERROR)
	{
		uart_print_nl("Errror countdown is down...");
		set_mode_s_error();
	}
	else if (c == ERROR)
		set_mode_r_error();
	else 
		set_mode_win();
}

void p_win(unsigned char c) {
	if ( c != WIN && c != ERROR)
	{
		uart_print_nl("Error during game");
		set_mode_s_error();
	}
	else if (c == ERROR)
		set_mode_r_error();
	else 
		set_mode_lose();
}

void i2c_lobby() {
	switch (TW_STATUS) {
	case TW_START:
		twi_busy = 1;
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(nb_ready);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty");
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		set_mode_s_error();
		break;
	case TW_MT_DATA_NACK:
		nb_ready++;
		if (nb_ready == nb_players)
			set_mode_countdown();
		else
			set_mode_ready();
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		twi_busy = 1;
		//Clear TWEA to nack data byte
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		twi_busy = 0;
		TWCR |= (1 << TWEA) | (1 << TWINT);
		n_ready(TWDR);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_ready(void) {
	switch (TW_STATUS) {
	case TW_SR_GCALL_ACK:
		twi_busy = 1;
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		if (nb_ready + 1 == nb_players) {
			set_mode_countdown();
		}
		twi_busy = 0;
		TWCR |= (1 << TWEA) | (1 << TWINT);
		n_ready(TWDR);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_countdown(void)
{
	switch (TW_STATUS) {
	case TW_START:
		twi_busy = 1;
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(LOSE);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty during countdown");
		set_mode_s_error();
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		break;
	case TW_MT_DATA_NACK:
		set_mode_lose();
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		twi_busy = 1;
		break;
	case TW_SR_GCALL_DATA_NACK:
		twi_busy = 0;
		TWCR |= (1 << TWEA) | (1 << TWINT);
		p_lose(TWDR);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_playing(void) {
	switch (TW_STATUS) {
	case TW_START:
		twi_busy = 1;
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(WIN);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty during game");
		set_mode_s_error();
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		break;
	case TW_MT_DATA_NACK:
		set_mode_win();
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		twi_busy = 1;
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		TWCR |= (1 << TWEA) | (1 << TWINT);
		twi_busy = 0;
		p_win(TWDR);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_error(void) {
	switch (TW_STATUS) {
	case TW_START:
		twi_busy = 1;
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(ERROR);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty error not received");
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		set_mode_r_error();
		break;
	case TW_MT_DATA_NACK:
		i2c_stop();
		set_mode_r_error();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		twi_busy = 1;
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		//Set STA to broadcast error
		if (TWDR == ERROR) {
			set_mode_r_error();
			TWCR |= (1 << TWEA) | (1 << TWINT);
		}
		else
			TWCR |= (1 << TWEA) | (1 << TWSTA) | (1 << TWINT);
		twi_busy = 0;
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_end() {
	switch (TW_STATUS) {
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		twi_busy = 1;
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		if (TWDR == ERROR) {
			set_mode_r_error();
		}
		TWCR |= (1 << TWEA) | (1 << TWINT);
		twi_busy = 0;
		break;
	default:
		uart_print_nl("Unexpected status : ");
		print_twi_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

ISR(TWI_vect) {
	switch (game_status) {
	case lobby:
		i2c_lobby();
		break;
	case ready:
		i2c_ready();
		break;
	case countdown:
		i2c_countdown();
		break;
	case playing:
		i2c_playing();
		break;
	case s_error:
		i2c_error();
		break;
	default:
		i2c_end();
		break;
	}
}

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed) {
		if ((game_status == lobby || game_status == countdown || game_status == playing) && !twi_busy)
			i2c_start();
	}
	_delay_ms(20);
	EIFR |= (1 << INTF0);
}

ISR(PCINT2_vect) {	
	sw2_pressed = !sw2_pressed;
	if (sw2_pressed) {
		print_game_status();
	}
	_delay_ms(20);
	PCIFR |= (1 << PCIF2);
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	interrupt_init();
	while (1) {
		switch(game_status) {
			case lobby:
				break;
			case ready:
				break;
			case countdown:
				io_countdown();
				if (game_status == countdown && !twi_busy)
					set_mode_playing();
				break;
			case playing:
				break;
			case win:
				io_win();
				set_mode_lobby();
				break;
			case lose:
				io_lose();
				set_mode_lobby();
				break;
			case s_error:
				break;
			case r_error:
				io_error();
				set_mode_lobby();
				break;
		}

	}
}
