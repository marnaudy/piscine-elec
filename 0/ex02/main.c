#include <avr/io.h>
#include <util/delay.h>

void wait_500ms() {
	//Calculate number of clock cycles to wait 500 ms
	unsigned long int operations = F_CPU / 2;
	unsigned long int loops = operations / 32;
	//Each iteration through this loop lasts 32 CPU cycles
	while (loops) {
		loops--;
	}
}

int main() {
	//Set direction of B0 to output (set bit 0 of DDRB to 1). OR ensures nothing else is modified
	DDRB |= (1 << DDB0);
	while (1) {
		//Change value of bit 0 (denoted by PB0) of Port B
		PORTB ^= (1 << PB0);
		wait_500ms();
	}
}
