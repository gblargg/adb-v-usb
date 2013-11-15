ADB-USB adapter using USBASP board
==================================
This project converts a USBASP ISP programmer board into an ADB-to-USB converter for using old Mac keyboards on a modern computer. The USBASP programmer can be had for under $3, and has all the components needed for software-based USB. A second USBASP programmer can be used to reprogram the first. The code should be easily adaptable to similar hardware.

This hasn't undergone much testing by others.

See keymap.h to customize layout.


Features
--------
* Tested on M3501 (Apple Extended Keyboard II) and M0116 (Apple Keyboard).
* Power key wakes host.
* Needs about 3100 bytes of flash.


Design
------
* USBASP has an atmega8 running on a 12MHz crystal.

* V-USB implements USB entirely in software, and supports this hardware.

* The USBASP design exposes three pins that we can use: TXD and RXD on the ISP connector, and a pin on JP3 (clock select jumper). TX isn't useful for ADB because it has a 1K resistor in series, but RXD or the pin on JP3 works. So no need to physically modify the USBASP at all.

* The ADB code is timing-sensitive, and the V-USB code's interrupt handler can take up to 100us. So we wait for a V-USB interrupt (by putting the CPU to sleep), then disable interrupts while we run the timing-sensitive ADB code, which takes about 3.3ms. The synchronization ensures that we don't randomly interfere with V-USB. Testing shows that this doesn't disrupt USB activity or cause USB errors (dmesg on Linux shows nothing). This also serves to limit the ADB polling rate to 125Hz (8ms period).

* main.c initializes things and has main loop. This polls ADB, converts to USB report, and sends. Also handles idle updating and wake from suspend. adb_usb.h maintains list of pressed keys in report and handles locking caps key. keymap.h specifies the key layout for the ADB keyboard. adb.c handles the low-level ADB protocol. usb_keyboard.c handles the HID keyboard protocol. usbdrv/ handles the low-level USB protocol.

* A good part of main.c handles optimized splitting of ADB event into USB key reports. ADB events sometimes have a key down and key up for the same key; if handled by a single report, they would cancel out, thus they must be split. So n ADB events can potentially convert to 2n USB reports. The optimizations done in handle_adb() may be overkill and removed at some point.


Construction
------------
* The only thing to wire up is the ADB cable. It has GND, +5V, and data.

* The simplest approach is take the spare ISP cable from the USBASP being reprogrammed, cut it in half, and connect the ADB cable to one end.

* ADB +5V and GND go to ISP +5V and GND. ADB Data goes to ISP RX. A 1K resistor must also be connected between ADB Data and +5V. See pinouts below.

* Set up the USBASP programmer board for reprogramming. JP1 has two jumper positions; be sure it has a jumper connecting two closer to the metal crystal (+5V). This will allower it to be powered by the other reprogrammer. Jumper JP2 to allow external reprogramming. JP2 doesn't usually have posts soldered in, so you'll have to solder some or improvise a way to bridge the contacts. Now connect to another ISP programmer, e.g. a second USBASP programmer.

* Extract sources and change to their directory. If you've unlocked the caps lock key on your keyboard so that it's momentary like all the other keys, open config.h and uncomment the #define UNLOCKED_CAPS line.

* Execute "make flash". This will build the program and flash it to your USBASP programmer, converting it into an ADB-USB converter.

* Unplug the reprogrammed USBASP and connect ADB and a keyboard. Verify proper wiring.

* Connect reprogrammed USBASP and verify that it shows up and keyboard works.


Alternate hardware
------------------
* Instead of connecting ADB to the ISP connector, it can be connected/soldered to the jumper pins/pads for JP1 and JP3. This has the advantage of allowing everything to stay plugged into the computer and connected to the keyboard, allowing easy development/improvement of the code. So you can have the reprogrammed USBASP and your ISP programmer connected to two USB ports on the host, both connected together via the ISP cable, and the ADB keyboard wired up all at the same time, then just reflash as desired without touching anything. After reflashing the host will re-enumerate the device and the keyboard will start working with the updated code.

* JP1 provides +5V. The two pins closer to the metal crystal must be connected together if you'll be reprogramming, otherwise you can just use the pin closest to the metal crystal.

* JP3 provides GND and ADB Data. GND is closer to the USB connector.

* Edit config.h and reflash to use this new connection.


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
* Test boot mode, which is apparently used by the BIOS on boot up.
* Add support for tying TXD to RXD and using series 1K resistor that's on the USBASP board as a pull-up for ADB, eliminating the need for an external resistor.
* Still seems to be reset after host suspend/resume, which e.g. resets its xmodmap on Linux.
* Figure out why LEDs sometimes don't update during heavy typing. usbFunctionSetup() in usb_keyboard.c receives USBRQ_HID_SET_REPORT, but occasionally our usbFunctionWrite() doesn't get called.

-- 
Shay Green <gblargg@gmail.com>
