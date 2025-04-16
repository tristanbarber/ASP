# Assignment 7a

## Build Instructions

1. Use `make` to build the driver and the test app. Load the driver using `sudo insmod usbkbd.ko`
2. Use `lsusb` to find the keyboard you want to bind to the driver
3. echo the appropriate device numbers into `/sys/bus/usb/drivers/usbhid/unbind` and `/sys/bus/usb/drivers/usbkbd/bind`