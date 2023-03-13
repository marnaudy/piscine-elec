NAME	=	main

HEX		=	main.hex

BIN		=	main.bin

SRCS	=	main.c

RM		=	rm -f

F_CPU	=	1000000

all:		hex flash

hex:		${SRCS}
			avr-gcc ${SRCS} -DF_CPU=${F_CPU} -mmcu=atmega328p -o ${BIN}
			avr-objcopy -j .text -j .data -O ihex ${BIN} ${HEX}

%.hex: %.bin
	 $(OBJCOPY) -j .text -j .data -O ihex $< $@

flash:		${HEX}
			avrdude -p m328p -b 115200 -c arduino -P /dev/ttyUSB0 -U flash:w:${HEX}:i

clean:
			${RM} ${BIN}

fclean:		clean
			${RM} ${HEX}

re:			fclean all

.PHONY:		all clean fclean re
