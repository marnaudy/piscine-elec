#include <avr/io.h>

int main() {
	//Set direction of B0 to output (set bit 0 of DDRB to 1). OR ensures nothing else is modified
	DDRB |= (1 << DDB0);
	//Set bit 0 (denoted by PB0) of Port B to 1
	PORTB |= (1 << PB0);
}
