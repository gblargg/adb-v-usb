// Initialization, ADB polling synchronized with V-USB interrupts, power key wakeup

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"

#define DEBUG( e )
#include "config.h"

#include "adb_usb.h"

enum { tcnt1_hz = (F_CPU + 512) / 1024 };

#ifndef TIMSK
	#define TIMSK TIMSK1
	#define TICIE1 ICIE1
	#define TIFR TIFR1
#endif

static void timer1_init( void )
{
	TCCR1B = 5<<CS10; // 1024 prescaler
	TIMSK |= 1<<TOIE1;
}

static volatile bool usb_inactive;

ISR(TIMER1_OVF_vect)
{
	usb_inactive = true;
}

static unsigned idle_timer; // incremented at same rate as TCNT1

static void update_idle( void )
{
	enum { t1_from_idle = (tcnt1_hz * 4L + 500) / 1000 };
	static unsigned prev_time;
	if ( !keyboard_idle_period )
	{
		prev_time = idle_timer;
	}
	else
	{
		unsigned elapsed = idle_timer - prev_time;
		unsigned period = keyboard_idle_period * t1_from_idle;
		if ( elapsed >= period )
		{
			prev_time = idle_timer;
			usb_keyboard_touch();
		}
	}
}

static bool usb_was_reset;

void hadUsbReset( void )
{
	usb_was_reset = true;
}

static void handle_reset( void )
{
	// Give host time to negotiate with us before we go to only handling one
	// USB command every 8ms
	if ( usb_was_reset )
	{
		usb_was_reset = false;
		
		DEBUG( debug_log( 0x1c, 0, 0 ) );
		usb_keyboard_reset();
		while ( TCNT1 < tcnt1_hz / 2 )
			usbPoll();
	}
}

enum { inactive_timeout = tcnt1_hz / 4 }; // no USB activity signals host asleep or resetting

// Waits for USB activity. Returns true if timed out.
static bool wait_usb( void )
{
	// accumulate time since last change to TCNT1
	idle_timer += TCNT1 - -inactive_timeout;
	
	// Wake after timeout if no USB activity
	TCNT1 = -inactive_timeout;
	usb_inactive = false;
	sleep_enable();
	sleep_cpu();
	return usb_inactive;
}

// Waits until USB becomes active, host issues USB reset, or keyboard power key is pressed
static void while_usb_inactive( void )
{
	DEBUG( debug_log( 0x1a, 0, 0 ) );
	
	// Wait for USB activity without any interruption
	cli();
	usb_was_reset = false;
	USB_INTR_PENDING = 1<<USB_INTR_PENDING_BIT; 
	while ( !(USB_INTR_PENDING & (1<<USB_INTR_PENDING_BIT)) ) // stop on USB activity
	{
		// Check for USB reset
		usbPoll();
		if ( usb_was_reset )
			break;
		
		// Check power key periodically
		if ( TCNT1 >= inactive_timeout )
		{
			TCNT1 = 0;
			uint16_t keys = adb_host_kbd_recv();
			if ( (keys >> 8 & 0xFF) == 0x7F )
			{
				// USB SE0 to wake host
				USBOUT &= ~USBMASK;
				USBDDR |= USBMASK;
				_delay_ms( 10 );
				USBDDR &= ~USBMASK;
				usb_was_reset = true;
				break;
			}
		}
	}
	sei();
	
	DEBUG( debug_log( 0x1b, 0, 0 ) );
}

static void init( void )
{
	#ifdef clock_prescale_set
		clock_prescale_set( clock_div_1 );
	#endif
	
	// Reduce power usage
	// Avoid floating inputs on unused pins
	DDRB  = 0;
	PORTB = 0xFF;
	DDRC  = 0;
	PORTC = 0xFF;
	DDRD  = 0;
	PORTD = 0xFF;
	
	#if ADB_TXD_PULLUP
		PORTD |= 1<<1;
		DDRD  |= 1<<1;
	#endif
	
	adb_usb_init();
	timer1_init();
}

static void split_adb( uint16_t keys );

int main( void )
{
	init();
	usb_was_reset = false; // already handled
	
	uint8_t frame = 0;
	for ( ;; )
	{
		handle_reset();
		
		// Update while USB is idle before next interrupt
		if ( usb_keyboard_poll() )
			usb_keyboard_update();
		
		if ( wait_usb() )
		{
			while_usb_inactive();
			continue;
		}
		
		uint8_t synced_time = TCNT1L;
		usb_keyboard_update();
		
		// Poll ADB every two out of three frames
		if ( frame <= 1 )
		{
			// Delay second poll by half a frame
			enum { half_interrupt = 3800L * tcnt1_hz / 1000000 };
			if ( frame == 1 )
				while ( (uint8_t) (TCNT1L - synced_time) < half_interrupt )
					{ }
			
			split_adb( adb_usb_read() );
			update_idle();
			
			frame++;
		}
		else
		{
			// Every third frame, update LEDs instead of polling ADB
			// This also gives USB a chance to send LED updates
			adb_usb_update_leds();
			
			frame = 0;
		}
	}
	
	return 0;
}

// Splits pair of ADB key events into multiple USB reports if necessary
static void split_adb( uint16_t keys )
{
	static uint8_t adb_extra_ = 0xFF;
	
	// The three potential events (0xFF=none), listed in order of occurrence
	uint8_t key2 = adb_extra_;  // possibly == 0xFF
	uint8_t key1 = keys >> 8;   // != 0xFF
	uint8_t key0 = keys & 0xFF; // possibly == 0xFF
	
	// We've got three events, some or all of which could be for the same key.
	// In that case, we must send the updates over USB separately or lose a
	// key press. There are an exasperating number of possible situations
	// (uppercase = pressed, lowercase = released):
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
	
	// Parse extra now
	adb_extra_ = 0xFF;
	if ( key2 != 0xFF )
		adb_usb_handle( key2 );
	
	// See if no new events
	if ( keys == adb_host_nothing || keys == adb_host_error )
		return;
	
	// For some keys (e.g. power) the same event is in both bytes
	if ( key0 == key1 )
		key0 = 0xFF;
	
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
				usb_keyboard_update();
			
			// Second event matches extra, so save as new extra
			adb_extra_ = key0;
			adb_usb_handle( key1 );
			return;
		}
		
		if ( ((key2 ^ key1) & 0x7F) == 0 )
		{
			// AaB   AB  a
			// Aa-   A   a
			
			// First event matches extra, so save as new extra
			adb_extra_ = key1;
			if ( key0 != 0xFF )
				adb_usb_handle( key0 );
			return;
		}
		
		// No matches, so extra gets merged with new events
		// AB-   AB  -
		// ABC   ABC -
	}
	// -B-   B   -
	// -BC   BC  -
	// -Bb   B   b
	
	adb_usb_handle( key1 );
	
	if ( ((key1 ^ key0) & 0x7F) == 0 )
		adb_extra_ = key0;
	else if ( key0 != 0xFF )
		adb_usb_handle( key0 );
}
