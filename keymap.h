/* Maps ADB key codes to USB key codes */

#include "usb_keyboard.h"
#include "keycode.h"
#include <stdint.h>

// Initialize with given ADB keyboard model (handler ID)
void keymap_init( uint8_t keyboard_id );

// Handle raw ADB event byte. May ignore or generate multiple USB events
void keymap_handle_event( uint8_t event );

// Called by this module to generate press/release events
void keymap_usb_hook( uint8_t usb_code, uint8_t press );

// Called every 12ms after keyboard is polled and events are processed. Allows keymap
// to generate multi-key macros, etc.
void keymap_idle( void );
