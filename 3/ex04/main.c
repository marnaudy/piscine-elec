#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define MAX_INPUT_SIZE 100

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

int get_user_login(char *user, char *pass) {
	uart_printstr("Enter your login:\r\n");
	uart_printstr("\tusername: ");
	int i = 0;
	while (i < MAX_INPUT_SIZE) {
		user[i] = uart_rx();
		if (user[i] == 127 && i != 0) {
			uart_printstr("\b \b");
			i--;
			continue;
		}
		if (user[i] == '\r') {
			user[i] = 0;
			break;
		}
		if (user[i] >= 20 && user[i] < 127)
			uart_tx(user[i]);
		if (user[i] != 127)
			i++;
	}
	uart_printstr("\r\n");
	if (i == MAX_INPUT_SIZE)
		return (-1);
	uart_printstr("\tpassword: ");
	i = 0;
	while (i < MAX_INPUT_SIZE) {
		pass[i] = uart_rx();
		if (pass[i] == 127 && i != 0) {
			uart_printstr("\b \b");
			i--;
			continue;
		}
		if (pass[i] == '\r') {
			pass[i] = 0;
			break;
		}
		if (pass[i] >= 20 && pass[i] < 127)
			uart_tx('*');
		if (pass[i] != 127)
			i++;
	}
	uart_printstr("\r\n");
	if (i == MAX_INPUT_SIZE)
		return (-1);
	return (0);
}

int str_match(const char *str1, const char *str2) {
	int i = 0;
	while (str1[i] && str1[i] == str2[i]) {
		i++;
	}
	return (str1[i] == str2[i]);
}

void set_leds_on() {
	PORTB |= ((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
}

void set_leds_off() {
	PORTB &= ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4));
}

void display_led_animation() {
	PORTB |= (1 << PB0);
	_delay_ms(250);
	PORTB |= (1 << PB1);
	_delay_ms(250);
	PORTB |= (1 << PB2);
	_delay_ms(250);
	PORTB |= (1 << PB4);
	_delay_ms(250);
	set_leds_off();
	_delay_ms(250);
	for (int i = 0; i < 3; i++) {
		set_leds_on();
		_delay_ms(250);
		set_leds_off();
		_delay_ms(250);
	}
}

int main() {
	char *user = "maxime";
	char *pass = "password";
	char user_input[MAX_INPUT_SIZE];
	char pass_input[MAX_INPUT_SIZE];
	uart_init();
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	while (1) {
		if (get_user_login(user_input, pass_input)) {
			uart_printstr("Input is too long\r\n\r\n");
			continue;
		}
		if (str_match(user, user_input) && str_match(pass, pass_input)) {
			uart_printstr("Hello ");
			uart_printstr(user);
			uart_printstr("!\r\nShall we play a game\r\n\r\n");
			display_led_animation();
		} else {
			uart_printstr("Bad combinaison username/password\r\n\r\n");
		}
	}
}
