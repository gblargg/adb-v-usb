ADB-USB adapter using USBASP board
==================================
This project converts a USBASP programmer board into an ADB-to-USB converter for using old Mac keyboards on a modern computer. The USBASP programmer can be had for under $3, and has all the components needed for software-based USB. A second USBASP programmer can be used to reprogram the first. The code should be easily adaptable to similar hardware.

I've been using this for months as my main keyboard interface.


Files
-----
	usbdrv/					V-USB library
	usbconfig.h
	usb_keyboard.c			Keyboard HID implementation
	usb_keyboard.h	
	usb_keyboard_event.h	Turns key press/release events into report structure
	adb.c					ADB protocol driver
	adb.h			
	adb_usb.h				ADB locking caps lock, misc
	keycode.h				
	keymap.h				ADB to USB key code conversion
	user_keymap.h			Key layouts for extended and compact ADB keyboards. Modify as needed.
	main.c					Main loop, ADB polling, suspend handling, boot protocol
	config.h				Configuration. Modify as needed.
	Makefile				Builds program
	README.md				Documentation
	CHANGES.md		
	LICENSE.txt		


Features
--------
* Tested on M3501 (Apple Extended Keyboard II) and M0116 (Apple Keyboard).
* Power key wakes host from sleep/suspend.
* Needs about 3400 bytes of flash.


Customization
-------------
config.h sets some keyboard options and how ADB is connected.

user_keymap.h to customizes keyboard layout. There are separate layouts for the extended and compact keyboard models.


Design
------
* USBASP has an atmega8 running on a 12MHz crystal.

* V-USB implements USB entirely in software.

* The USBASP design exposes three pins that we can use, in addition to the four ISP pins used for flashing the device (RESET, SCK, MOSI, MISO): TXD and RXD on the ISP connector, and a pin on JP3 (clock select jumper). TX isn't useful for ADB because it has a 1K resistor in series, but RXD or the pin on JP3 work.

* The ADB code is timing-sensitive, and the V-USB code's interrupt handler can take up to 100us, so we wait for a V-USB interrupt (by putting the CPU to sleep), then disable interrupts while we run the timing-sensitive ADB code, which takes about 3.3ms. The synchronization ensures that we don't randomly interfere with V-USB. Testing shows that this doesn't disrupt USB activity or cause USB errors (dmesg on Linux shows nothing). This also serves to limit the ADB polling rate to 125Hz (8ms period). The ADB polling rate is further slowed to 83Hz (12ms period) to match the rate a Mac does. Some keyboards also can't handle a higher rate reliably.

* ADB events sometimes have a key down and key up for the same 16-bit event; if handled by a single USB keyboard report, they would cancel out, thus they must be split into two separate reports. So n ADB events can potentially convert to 2n USB reports. The optimizations done in handle_adb() may be overkill and removed at some point.


Construction
------------
* The only thing to wire up is the ADB cable. It has GND, +5V, and data.

* The simplest approach is take the spare ISP cable from the USBASP being reprogrammed, cut it in half, strip the ends, and connect the ADB cable to one end.

* ADB +5V and GND go to ISP +5V and GND. ADB Data goes to ISP RX. A 1K resistor must also be connected between ADB Data and +5V. See pinouts below.

* Set up the USBASP programmer board for reprogramming. JP1 has two jumper positions; be sure it has a jumper connecting two closer to the metal crystal (+5V). This will allowe it to be powered by the other reprogrammer. Jumper JP2 to allow external reprogramming. JP2 doesn't usually have posts soldered in, so you'll have to solder some or improvise a way to bridge the contacts. Now connect to another ISP programmer, e.g. a second USBASP programmer.

* Extract sources and change to their directory. Modify config.h as desired.

* Execute "make flash". This will build the program and flash it to your USBASP programmer, converting it into an ADB-USB converter.

* Unplug the reprogrammed USBASP and connect ADB and a keyboard. Verify proper wiring.

* Plug the reprogrammed USBASP into a PC and verify that it shows up and keyboard works.


Pinouts
-------
USBASP ISP connector (male):

           ______
	 MOSI | 1  2 | +5V
	  GND | 3  4 | TXD
	RESET   5  6 | RXD
	  SCK | 7  8 | GND
	 MISO | 9 10 | GND
           ------

ADB cable (male):

	       ,-.,-.
	  +5V / 3  4 \ GND
	Data | 1    2 | Power key
	      \  ==  /
	       ------


To do
-----
* Figure out why LEDs sometimes don't update during heavy typing. usbFunctionSetup() in usb_keyboard.c receives USBRQ_HID_SET_REPORT, but occasionally our usbFunctionWrite() doesn't get called.

-- 
Shay Green <gblargg@gmail.com>
