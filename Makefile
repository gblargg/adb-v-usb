all:
	avr-gcc -mmcu=atmega8 -DF_CPU=12000000 -DHAVE_CONFIG_H \
		-Os -o main.elf -I. *.c usbdrv/usbdrv.c usbdrv/usbdrvasm.S
	avr-objcopy -R .eeprom -R .fuse -R .lock -R .signature -O ihex main.elf main.hex

flash: all
	avrdude -v -p m8 -c usbasp -e -U main.hex
