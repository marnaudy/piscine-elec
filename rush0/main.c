#include <avr/io.h>
#include <util/twi.h>

#define TWI_BAUDRATE 100000ul

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

void uart_printstr_nl(const char *str) {
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
	TWCR |= (1 << TWSTA) | (1 << TWINT);
}

void i2c_stop() {
	//Make sure start bit is clear
	TWCR &= ~(1 << TWSTA);
	//Set stop bit
	TWCR |= (1 << TWSTO);
	//Clear TWINT bit (notifies module to continue)
	TWCR |= (1 << TWINT);
}

void io_init() {
	//Set LEDs to output
	DDRB = (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	DDRD = (1 << DDD3) | (1 << DDD5) | (1 << DDD6);
	//Switch LED to red for not ready state
	PORTD |= (1 << PD6);
}

void set_rgb(char colour) {
	//Reset switch all rgb off
	PORTD &= ((1 << PD3) | (1 << PD5) | (1 << PD6));
	if (colour == 'R') {
		PORTD |= (1 << PD6);
	}
	if (colour == 'G') {
		PORTD |= (1 << PD5);
	}
	if (colour == 'B') {
		PORTD |= (1 << PD3);
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

enum g_stat {
	lobby,
	ready,
	countdown,
	playing,
	lose,
	win
};

int nb_players = 2;
int nb_ready = 0;
enum g_stat game_status = lobby;

ISR(INT0_vect) {
	switch (game_status) {
	case lobby:
	case countdown:
	case playing:
		i2c_start();
		break;
	default:
		break;
	}
}

void i2c_write(unsigned char data) {
	TWDR = data;
	//Clear start flag
	TWCR &= ~(1 << TWSTA);
	//Clear interrupt
	TWCR |= (1 << TWINT);
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
	case TW_MT_DATA_ACK:
		uart_print_nl("Data has been acknoledged, weird...");
		i2c_stop();
		break;
	case TW_MT_DATA_NACK:
		set_rgb('G');
		nb_ready++;
		game_status = ready;
		i2c_stop();
		break;
	case TW_SR_GCALL_ACK://Add lost arbitration
		TWCR &= ~(1 << TWEA);
		TWCR |= (1 << TWINT);
	case TW_SR_DATA_NACK:
		ft_nready(TWDR);
		TWCR |= (1 << TWEA) | (1 << TWINT);
	}
}

ISR(TWI_vect) {
	switch (game_status) {
	case lobby:
		i2c_lobby();
		break;
	case ready:
		i2c_ready();
	case countdown:
		i2c_countdown();
	case playing:
		i2c_playing();
		break;
	default: //lose and win 
		break;
	}
}

int main() {
	uart_init();
	i2c_init();
	io_init();
	interrupt_init();
	while (1) {
		while (nb_ready != nb_players) {};
	}
}
