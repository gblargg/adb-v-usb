#include "usb_keyboard.h"

#include <stdbool.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"

uint8_t keyboard_report_ [8];
uint8_t keyboard_idle_period;
uint8_t keyboard_leds;

const PROGMEM char usbHidReportDescriptor [USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
	0x05, 0x01, // USAGE_PAGE (Generic Desktop)
	0x09, 0x06, // USAGE (Keyboard)
	0xa1, 0x01, // COLLECTION (Application)
	0x05, 0x07, //	 USAGE_PAGE (Keyboard)
	0x19, 0xe0, //	 USAGE_MINIMUM (Keyboard LeftControl)
	0x29, 0xe7, //	 USAGE_MAXIMUM (Keyboard Right GUI)
	0x15, 0x00, //	 LOGICAL_MINIMUM (0)
	0x25, 0x01, //	 LOGICAL_MAXIMUM (1)
	0x75, 0x01, //	 REPORT_SIZE (1)
	0x95, 0x08, //	 REPORT_COUNT (8)
	0x81, 0x02, //	 INPUT (Data,Var,Abs)
	0x95, 0x01, //	 REPORT_COUNT (1)
	0x75, 0x08, //	 REPORT_SIZE (8)
	0x81, 0x03, //	 INPUT (Cnst,Var,Abs)
	0x95, 0x05, //	 REPORT_COUNT (5)
	0x75, 0x01, //	 REPORT_SIZE (1)
	0x05, 0x08, //	 USAGE_PAGE (LEDs)
	0x19, 0x01, //	 USAGE_MINIMUM (Num Lock)
	0x29, 0x05, //	 USAGE_MAXIMUM (Kana)
	0x91, 0x02, //	 OUTPUT (Data,Var,Abs)
	0x95, 0x01, //	 REPORT_COUNT (1)
	0x75, 0x03, //	 REPORT_SIZE (3)
	0x91, 0x03, //	 OUTPUT (Cnst,Var,Abs)
	0x95, 0x06, //	 REPORT_COUNT (6)
	0x75, 0x08, //	 REPORT_SIZE (8)
	0x15, 0x00, //	 LOGICAL_MINIMUM (0)
	0x25, 0x65, //	 LOGICAL_MAXIMUM (101)
	0x05, 0x07, //	 USAGE_PAGE (Keyboard)
	0x19, 0x00, //	 USAGE_MINIMUM (Reserved (no event indicated))
	0x29, 0x65, //	 USAGE_MAXIMUM (Keyboard Application)
	0x81, 0x00, //	 INPUT (Data,Ary,Abs)
	0xc0        // END_COLLECTION  
};

uint8_t usbFunctionWrite( uint8_t data [], uint8_t len )
{
	(void) len;
	keyboard_leds = data [0];
	return 1;
}

uint8_t usbFunctionSetup( uint8_t data [8] )
{
	usbRequest_t const* rq = (usbRequest_t const*) data;

	if ( (rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS )
		return 0;
	
	static uint8_t protocol = 1; //	0=boot 1=report
	
	switch ( rq->bRequest )
	{
	case USBRQ_HID_GET_REPORT: // perhaps also only used for boot protocol
		usbMsgPtr = keyboard_report_;
		return sizeof keyboard_report_;
	
	case USBRQ_HID_SET_REPORT:
		if ( rq->wLength.word != 1 )
			return 0;
		return USB_NO_MSG; // causes call to usbFunctionWrite
	
	// The following are only used for boot protocol:
	case USBRQ_HID_GET_IDLE:
		usbMsgPtr = &keyboard_idle_period;
		return 1;
	
	case USBRQ_HID_SET_IDLE:
		keyboard_idle_period = rq->wValue.bytes [1];
		return 0;
	
	case USBRQ_HID_GET_PROTOCOL:
		usbMsgPtr = &protocol;
		return 1;
	
	case USBRQ_HID_SET_PROTOCOL:
		protocol = rq->wValue.bytes [1];
		return 0;
	
	default:
		return 0;
	}
}

void usb_init( void )
{
	// INT0 = input with no pull-up
	PORTD &= ~(1<<2);
	DDRD  &= ~(1<<2);
	
	// Apparetly USB pins must be low normally
	USBOUT &= ~USBMASK;
	
	// Force USB re-enumeration in case we're being re-run while connected
	usbDeviceDisconnect();
	_delay_ms( 250 );
	usbDeviceConnect();
	
	usbInit();
}

uint8_t usb_configured( void )
{
	usb_keyboard_poll();
	return usbConfiguration;
}

uint8_t usb_keyboard_poll( void )
{
	sei(); // so caller doesn't ever even have to enable interrupts
	usbPoll();
	return usbInterruptIsReady();
}

int8_t usb_keyboard_send( void )
{
	if ( !usbInterruptIsReady() )
		while ( !usb_keyboard_poll() )
			{ }
	
	// copies report so we don't have to worry about caller changing it before USB uses it
	usbSetInterrupt( keyboard_report_, sizeof keyboard_report_ );
	
	#if 0
	debug_byte( keyboard_modifier_keys );
	for ( byte i = 0; i < 6; i++ )
		debug_byte( keyboard_keys [i] );
	debug_newline();
	#endif
	
	return 0;
}
