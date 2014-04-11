* Fixed to not give any low-level USB errors when PC is resuming from sleep
* Improved power key to respond immediately and wake PC
* Fixed boot protocol mode to work properly after reboot (used when booting PC into BIOS setup program)
* Added detection of extended versus compact keyboard, and use of separate keymap.


blargg-adb-v-usb-4
------------------
* Fixed keypad equals to KP_EQUAL instead of normal equals.
* Fixed to once again distinguish between left/right shift, control, and alt keys. This got broken in previous version.


blargg-adb-v-usb-3
------------------
* Now merges extra ADB events with next where possible, making better use of limited USB update opportunities.
* Eliminated adjustable ADB polling rate, as higher rates only hurt, and improved USB synchronization made adjustable polling impossible.
* Consolidated sources.
* Using 16-bit timer1 for everything.


blargg-adb-v-usb-2
------------------
* Keyboard works after resume from sleep.
* Power key wakes host.
* Changed polling to 12ms (83Hz). Works better with M0116. Can still be adjusted back to 8ms (125Hz) in main.c
* Fixed dropped key when it's pressed and released in same ADB event.
* Reorganized code and made USB synchronization more robust.


blargg-adb-v-usb-1
------------------
* First release.
