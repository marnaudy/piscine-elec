#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

uint8_t sw1_pressed = 0;
uint8_t sw2_pressed = 0;
uint8_t n = 0;
uint8_t *value_ptr = 0;
uint8_t *active_ptr = (uint8_t *) (uint16_t) 4;
uint8_t active_counter = 0;

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

ISR(INT0_vect) {
	sw1_pressed = !sw1_pressed;
	if (sw1_pressed) {
		n++;
		display(n);
		//Temporarily disable interrupts to update counter value
		SREG &= ~(1 << SREG_I);
		eeprom_write_byte(value_ptr, n);
		SREG |= (1 << SREG_I);
	}
	//Wait for signal to stabilise
	_delay_ms(10);
	//Reset interrupt flag
	EIFR &= (1 << INTF0);
}

ISR(PCINT2_vect) {
	sw2_pressed = !sw2_pressed;
	if (sw2_pressed) {
		active_counter++;
		if (active_counter == 4) {
			active_counter = 0;
		}
		value_ptr = (uint8_t *) (uint16_t) active_counter;
		//Temporarily disable interrupts
		n = eeprom_read_byte(value_ptr);
		SREG &= ~(1 << SREG_I);
		eeprom_write_byte(active_ptr, active_counter);
		SREG |= (1 << SREG_I);
		display(n);
	}
	//Wait for signal to stabilise
	_delay_ms(10);
	//Reset interrupt flag
	PCIFR &= (1 << PCIF2);
}

int main() {
	//Wait for EEPROM to be ready
	while (SPMCSR & (1 << SELFPRGEN)) {};
	active_counter = eeprom_read_byte(active_ptr);
	value_ptr = (uint8_t *) (uint16_t) active_counter;
	n = eeprom_read_byte(value_ptr);
	display(n);
	//Set LEDs to ouput
	DDRB |= (1 << DDB0) | (1 << DDB1) | (1 << DDB2) | (1 << DDB4);
	//Enable interrupt on logical change of SW1
	SREG |= (1 << SREG_I);
	EIMSK |= (1 << INT0);
	EICRA |= (1 << ISC00);
	//Enable interrupt on SW2
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT20);
	while (1) {}
}
