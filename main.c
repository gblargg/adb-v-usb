// Initialization, ADB polling synchronized with V-USB interrupts, power key wakeup

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"

typedef unsigned char byte;
#define DEBUG( e )
#include "config.h"

#include "adb_usb.h"

enum { tcnt1_hz = (F_CPU + 512) / 1024 };
enum { wake_period = tcnt1_hz }; // once a second

static void timer1_init( void )
{
	TCCR1B = 5<<CS10; // 1024 prescaler
	TIMSK |= 1<<TOIE1;
}

static volatile bool wake_ignore = true; // we set this regularly in main loop

// Runs once every several seconds
ISR(TIMER1_OVF_vect)
{
	if ( wake_ignore )
	{
		wake_ignore = false;
	}
	else
	{
		// Host is in suspend, so have this ISR called more often
		TCNT1 = -wake_period;
	
		uint16_t keys = adb_host_kbd_recv();
	
		// See if power key pressed
		if ( (keys >> 8 & 0xFF) == 0x7F )
		{
			// USB SE0 to wake host
			USBOUT &= ~USBMASK;
			USBDDR |= USBMASK;
			_delay_ms( 10 );
			USBDDR &= ~USBMASK;
			GIFR = 1<<INTF0; // clear since we probably just triggered it
		}
	}
}

static bool update_idle( void )
{
	// TODO: get host to actually use this mode so it can be tested
	if ( keyboard_idle_period ) // update periodically if USB host wants it
	{
		enum { t1_from_idle = (tcnt1_hz * 4L + 500) / 1000 };
		static unsigned next;
		if ( (int) (TCNT1 - next) >= 0 )
		{
			next = TCNT1 + keyboard_idle_period * t1_from_idle;
			return true;
		}
	}
	return false;
}

static bool handle_adb( void )
{
	static byte adb_extra_ = 0xFF;
	
	cli();
	uint16_t keys = adb_host_kbd_recv();
	sei();
	
	bool changed = release_caps();
	
	// The three potential events (0xFF=none), listed in order of occurrence
	byte key2 = adb_extra_;  // possibly == 0xFF
	byte key1 = keys >> 8;   // != 0xFF
	byte key0 = keys & 0xFF; // possibly == 0xFF
	
	// Parse extra now
	adb_extra_ = 0xFF;
	if ( key2 != 0xFF )
	{
		parse_adb( key2 );
		changed = true;
	}
	
	// See if no new events
	if ( keys == adb_host_nothing || keys == adb_host_error )
		return changed;
	
	// For some keys (e.g. power) the same event is in both bytes
	if ( key0 == key1 )
		key0 = 0xFF;
	
	// We've got three events, some or all of which could be for the same key.
	// In that case, we must send the updates over USB separately or lose a
	// key press. There are an exasperating number of possible situations:
	//
	// Key 
	// 210 Sends Extra
	// ---------------
	// -B-   B   -
	// -BC   BC  -
	// -Bb   B   b
	// AB-   AB  -
	// ABC   ABC -
	// ABa   AB  a
	// AaB   AB  a
	// Aa-   A   a
	// AaA A a   A
	
	// Cases are listed below where they are handled
		
	if ( key2 != 0xFF )
	{
		if ( ((key2 ^ key0) & 0x7F) == 0 )
		{
			// ABa   AB  a
			// AaA A a   A
			
			// If both new events are same as extra key, we must send extra,
			// first event, then save second event as new extra
			if ( ((key2 ^ key1) & 0x7F) == 0 )
				usb_keyboard_send();
			
			// Second event matches extra, so save as new extra
			adb_extra_ = key0;
			parse_adb( key1 );
			return true;
		}
		
		if ( ((key2 ^ key1) & 0x7F) == 0 )
		{
			// AaB   AB  a
			// Aa-   A   a
			
			// First event matches extra, so save as new extra
			adb_extra_ = key1;
			if ( key0 != 0xFF )
				parse_adb( key0 );
			return true;
		}
		
		// No matches, so extra gets merged with new events
		// AB-   AB  -
		// ABC   ABC -
	}
	// -B-   B   -
	// -BC   BC  -
	// -Bb   B   b
	
	parse_adb( key1 );
	
	if ( ((key1 ^ key0) & 0x7F) == 0 )
		adb_extra_ = key0;
	else if ( key0 != 0xFF )
		parse_adb( key0 );
	
	return true;
}

// Avoid floating inputs on unused pins
static void pullup_ports( void )
{
	DDRB  = 0;
	PORTB = 0xFF;
	DDRC  = 0;
	PORTC = 0xFF;
	DDRD  = 0;
	PORTD = 0xFF;
}

int main( void )
{
	// Reduce power usage
	pullup_ports();
	
	adb_usb_init();
	timer1_init();
	
	byte frame = 0;
	bool changed = false;
	for ( ;; )
	{
		// Flush changes and synchronize
		if ( usb_keyboard_poll() && changed )
		{
			// USB is idle so do this before interrupt
			usb_keyboard_send();
			changed = false;
		}
		sleep_enable();
		sleep_cpu();
		byte synced_time = TCNT1L;
		if ( changed )
			usb_keyboard_send();
		changed = false;
		
		// Poll ADB every two out of three frames
		if ( frame <= 1 )
		{
			// Delay second poll by half a frame
			enum { half_interrupt = 3800L * tcnt1_hz / 1000000 };
			if ( frame == 1 )
				while ( (byte) (TCNT1L - synced_time) < half_interrupt )
					{ }
			
			// Poll ADB and send any changes
			// DO NOT use || as this will not call both funcs!
			changed = handle_adb() | update_idle();
			
			frame++;
		}
		else
		{
			// Every third frame, update LEDs instead of polling ADB
			// This also gives USB a chance to send LED updates
			handle_leds();
			wake_ignore = true;
			
			frame = 0;
		}
	}
}
