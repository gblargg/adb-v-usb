// Conversion from ADB to USB, including locking caps to momentary caps USB uses

// Init ADB reading and initialize keyboard
static void adb_usb_init( void );

// Parse an ADB key press/release byte and update keyboard_modifiers and keyboard_keys.
static void parse_adb( unsigned char raw );

// Call before each ADB transaction to process previous press of caps lock.
static void release_caps( void );

// Call periodically to update LED state
static void handle_leds( void );

#include "keymap.h"
#include "adb.h"
#include "usb_keyboard.h"

enum { max_keys = 6 };
enum { released_mask = 0x80 };
enum { adb_caps = 0x39 };

bool usb_changed;

static void parse_adb_( byte raw )
{
	keymap_handle_event( raw );
}

// called by keymap_handle_event()
void keymap_usb_hook( uint8_t code, uint8_t released )
{
	if ( !code )
		return;
	
	usb_changed = true;
	
	if ( KC_LCTRL <= code && code <= KC_RGUI )
	{
		// Modifier; route to mask rather than keys list
		byte mask = 1 << (code - KC_LCTRL);
		keyboard_modifier_keys |= mask;
		if ( released )
			keyboard_modifier_keys ^= mask;
	}
	else
	{
		// Find key
		byte* p = keyboard_keys + 6;
		do
		{
			if ( *--p == code )
				break;
		}
		while ( p != keyboard_keys );

		if ( released )
		{
			// Remove key
			if ( *p == code )
			{
				*p = 0;
				return;
			}
			
			DEBUG(debug_byte( code ));
			DEBUG(debug_str( "released key not in list\n" ));
		}
		else
		{
			// Don't add if already there (sometimes keyboard gives multiple
			// key down events when pressing lots of keys)
			if ( *p == code )
			{
				DEBUG(debug_str( "pressed key already in list\n" ));
				return;
			}
			
			// Add to first empty entry
			p = keyboard_keys + 6;
			do
			{
				if ( *--p == 0 )
				{
					*p = code;
					return;
				}
			}
			while ( p != keyboard_keys );
			
			DEBUG(debug_str( "too many keys pressed\n" ));
		}
	}
}

#ifdef UNLOCKED_CAPS

static void parse_adb( byte raw )
{
	parse_adb_( raw );
}

static void release_caps( void ) { }

static void leds_changed( byte leds ) { (void) leds; }

#else

enum { caps_mask = 2 };

static byte caps_pressed;
static byte caps_on;

static void release_caps( void )
{
	if ( caps_pressed )
	{
		caps_pressed = false;
		parse_adb_( adb_caps | released_mask );
	}
}

static void leds_changed( byte leds )
{
	// only update our flag when USB host changes caps LED
	static byte prev_caps_led;
	byte caps_led = leds & caps_mask;
	if ( prev_caps_led != caps_led )
	{
		prev_caps_led = caps_led;
		caps_on = caps_led;
	}
}

static void parse_adb( byte raw )
{
	if ( (raw & 0x7F) != adb_caps )
	{
		parse_adb_( raw );
		return;
	}
	
	byte new_caps = (raw & released_mask) ? 0 : caps_mask;
	if ( caps_on != new_caps )
	{
		// in case host doesn't handle LED, keep ours updated
		caps_on = new_caps;
		caps_pressed = 1;
		parse_adb_( adb_caps );
	}
}

#endif

static void adb_usb_init( void )
{
	adb_host_init();
	_delay_ms( 300 ); // keyboard needs at least 250ms or it'll ignore host_listen below
	
	uint16_t id = adb_host_talk( adb_cmd_read + 3 );
	if ( id == adb_host_nothing || id == adb_host_error )
		id = 0;
	keymap_init( id & 0xff );
	
	// Enable separate key codes for left/right shift/control/option keys
	// on Apple Extended Keyboard.
	adb_host_listen( adb_cmd_write + 3, 0x02, 0x03 );
	
	usb_init();
	while ( !usb_configured() )
		{ }
}

static void handle_leds( void )
{
	static byte leds = -1;
	byte new_leds = keyboard_leds;
	if ( leds != new_leds )
	{
		leds = new_leds;
		leds_changed( new_leds );
		
		cli();
		adb_host_kbd_led( ~new_leds & 0x07 );
		sei();
	}
}
