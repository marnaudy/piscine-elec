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

volatile int nb_players = 2;
volatile int nb_ready = 0;
volatile enum g_stat game_status = lobby;

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

void io_countdown(void) {
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
	//Enable interrupts on SW2 (PCINT20)
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
	//Enable interrupts on TWI
	TWCR |= (1 << TWIE);
}

void send_status() {
	char *base = "0123456789ABCDEF";
	int status = TW_STATUS;
	uart_printstr("0x");
	uart_tx(base[status / 16]);
	uart_tx(base[status % 16]);
	uart_printstr("\r\n");
}

void n_ready(unsigned char c) {
	if (c != nb_ready && c != ERROR)
	{
		uart_print_nl("Error nb players");
		game_status = s_error;
		TWCR |= (1 << TWSTA);
	}
	else if (c == ERROR)
		game_status = r_error;
	nb_ready++;
}

void p_lose(unsigned char c) {
	if (c != LOSE && c != ERROR)
	{
		uart_print_nl("Errror countdown is down...");
		game_status = s_error;
		TWCR |= (1 << TWSTA);
	}
	else if (c == ERROR)
		game_status = r_error;
	else 
		game_status = win;
}

void p_win(unsigned char c) {
	if ( c != WIN && c != ERROR)
	{
		uart_print_nl("Error during game");
		game_status = s_error;
		TWCR |= (1 << TWSTA);
	}
	else if (c == ERROR)
		game_status = r_error;
	else 
		game_status = lose;
}

void i2c_error(void) {
	switch (TW_STATUS) {
	case TW_START:
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
		game_status = r_error;
		break;
	case TW_MT_DATA_NACK:
		i2c_stop();
		game_status = r_error;
		break;
	default:
		uart_print_nl("Unexpected status : ");
		send_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_ready(void) {
	switch (TW_STATUS) {
	case TW_SR_GCALL_ACK:
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		if (nb_ready + 1 == nb_players) {
			game_status = countdown;
			set_rgb('B');
		}
		n_ready(TWDR);
		TWCR |= (1 << TWEA) | (1 << TWINT);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		send_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_playing(void) {
	switch (TW_STATUS) {
	case TW_START:
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(WIN);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty during game");
		game_status = s_error;
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		break;
	case TW_MT_DATA_NACK:
		game_status = win;
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		p_win(TWDR);
		TWCR |= (1 << TWEA) | (1 << TWINT);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		send_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_countdown(void)
{
	switch (TW_STATUS) {
	case TW_START:
		i2c_write(TW_WRITE);
		break;
	case TW_MT_SLA_ACK:
		i2c_write(LOSE);
		break;
	case TW_MT_SLA_NACK:
		uart_print_nl("Lobby empty during countdown");
		game_status = s_error;
		i2c_stop();
		break;
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknowledged, weird...");
		i2c_stop();
		break;
	case TW_MT_DATA_NACK:
		game_status = lose;
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		p_lose(TWDR);
		TWCR |= (1 << TWEA) | (1 << TWINT);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		send_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

void i2c_lobby() {
	switch (TW_STATUS) {
	case TW_START:
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
		break;
	case TW_MT_DATA_NACK:
		nb_ready++;
		set_rgb('G');
		if (nb_ready == nb_players)
		{
			game_status = countdown;
			set_rgb('B');
		}
		else
			game_status = ready;
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK:
	case TW_SR_ARB_LOST_GCALL_ACK:
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
		break;
	case TW_SR_GCALL_DATA_NACK:
		n_ready(TWDR);
		TWCR |= (1 << TWEA) | (1 << TWINT);
		break;
	default:
		uart_print_nl("Unexpected status : ");
		send_status();
		TWCR |= (1 << TWINT);
		break;
	}
}

ISR(TWI_vect) {
	send_status();
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
		break;
	}
}

ISR(INT0_vect) {
	uart_print_nl("INT0 interrupt");
	if (game_status == lobby || game_status == countdown || game_status == playing)
		i2c_start();
	_delay_ms(250);
	EIFR |= (1 << INTF0);
}

ISR(PCINT2_vect) {
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

void reset_game(void) {

	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
	set_rgb('R') ;
	nb_ready = 0;
	game_status = lobby;
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
				if (game_status == countdown)
					game_status = playing;
				break;
			case playing:
				break;
			case win:
				io_win();
				reset_game();
				break;
			case lose:
				io_lose();
				reset_game();
				break;
			case s_error:
				break;
			case r_error:
				io_error();
				reset_game();
				break;
		}

	}
}
