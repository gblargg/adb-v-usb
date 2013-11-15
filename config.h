#ifndef CONFIG_H
#define CONFIG_H

// Uncomment if you have removed the locking mechanism from your caps lock
//#define UNLOCKED_CAPS 1

// Configured to use RXD (ICSP pin 6, next to /RESET pin) for ADB data
// Change to 1 to use JP3 for ADB data
#if 0
	// JP3
	#define ADB_PORT PORTC
	#define ADB_PIN  PINC
	#define ADB_DDR  DDRC
	#define ADB_DATA_BIT 2
#else
	// RXD
	#define ADB_PORT PORTD
	#define ADB_PIN  PIND
	#define ADB_DDR  DDRD
	#define ADB_DATA_BIT 0
#endif

// Shortens ADB bit cells by 25% (still within spec) to reduce chance of conflict
// with USB interrupts. Shouldn't cause any problems.
#define ADB_REDUCED_TIME 1

#endif
