/*
 ============================================================================
 Name        : usb_device.c
 Author      : amneiht
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include "usb_dc.h"
void simple_chat(usbdc_handle *han);

int main(void) {
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	usbdc_handle *han = usbdc_handle_create("/dev/ffs", 8, 1);
	simple_chat(han);
	usbdc_handle_free(han);
	return EXIT_SUCCESS;
}
void simple_chat(usbdc_handle *han) {
	usbdc_line *ls = han->line_array[0];
//	usbdc_line_fast_mode(ls);
	char buff[USB_MAX];
	char lms[] = "device test data";
	while (1) {

		while (!han->connect) {
			usbdc_handle_checkevt(han);
			usleep(100 * 1000);
		}
		while (han->connect) {
			usbdc_handle_checkevt(han);
			if (usbdc_line_is_read_ready(ls)) {
				int ret = usbdc_line_read(ls, buff, 1000);
				buff[ret] = '\0';
				puts(buff);
			} else if (usbdc_line_is_read_timeout(ls, 500)) {
				usbdc_line_read_cancel(ls);
			}
		}
	}
}
