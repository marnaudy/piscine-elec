NAME	=	main

HEX		=	main.hex

BIN		=	main.bin

SRCS	=	main.c

RM		=	rm -f

F_CPU	=	16000000ul

all:		hex flash

hex:		${HEX}

${BIN}:		${SRCS}
			avr-gcc ${SRCS} -DF_CPU=${F_CPU} -DUART_BAUDRATE=112500ul -mmcu=atmega328p -O -o ${BIN}

${HEX}: 	${BIN}
	 		avr-objcopy -j .text -j .data -O ihex ${BIN} ${HEX}

flash:		${HEX}
			avrdude -p m328p -b 115200 -c arduino -P /dev/ttyUSB0 -U flash:w:${HEX}:i

clean:
			${RM} ${BIN}

fclean:		clean
			${RM} ${HEX}

re:			fclean all

.PHONY:		all clean fclean re
