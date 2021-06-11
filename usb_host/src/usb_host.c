/*
 ============================================================================
 Name        : usb_host.c
 Author      : amneiht
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <usb_dc.h>
#include <unistd.h>
#include <string.h>
void simplechat(usbdc_handle *han);
int main(void) {
	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	usbdc_handle *han = usbdc_handle_new(0x0129, 0x00fa);
	if (han != NULL) {
		printf("data is : %d", han->nline);
		simplechat(han);
		usbdc_handle_free(han);
	} else
		puts("sai cmnr");
	return EXIT_SUCCESS;
}
void simplechat(usbdc_handle *han) {
	if (han == NULL) {
		puts("NULL cmnr");
		return;
	}
	usbdc_line *ls = han->line_array[0];
	usbdc_line_fast_mode(ls);
	char buff[USB_MAX];
	char lms[] = "host test data";
	while (han->connect) {
		usbdc_handle_checkevt(han);
//		if (usbdc_line_is_write_ready(ls)) {
//			int ret = usbdc_line_write(ls, lms, strlen(lms) + 1);
//			if (ret < 0) {
//				puts("no connect");
//			}
//		}
		if (usbdc_line_is_read_ready(ls)) {
			int read = usbdc_line_read(ls, buff, USB_MAX);
			fprintf(stderr, "device talk:%.*s \n", read, buff);
		}

		if (usbdc_line_is_write_timeout(ls, 3000)) {
			usbdc_line_write_cancel(ls);
			puts("write failse");
		}
		usleep(1000 * 100);
	}
}
#ifdef __ARM_EABI_
#endif
