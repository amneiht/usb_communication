/*
 * test_stream.c
 *
 *  Created on: Jun 28, 2021
 *      Author: amneiht
 */

#include <usbdc_stream.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
void test_stream(usbdc_stream *ls);

int main(int argc, char **argv) {
//	usbdc_stack *bf = usbdc_stack_new(100, 2000);
//	usbdc_message mess, mess2;
//
//	mess.length = sprintf(mess.line_buf,
//			"eee tyge45ypika grrr uyhtu6 5y6eatest dcdsh");
//	mess.length = 2000;
//	int i = 8;
//	while (i > 0) {
//		i--;
//		usbdc_stack_push(bf, mess.line_buf, mess.length);
//		mess2.length = usbdc_stack_package_pop(bf, mess2.line_buf, USB_MAX);
//	}
//	usleep(200 * 1000);
	usbdc_stream *ls = usbdc_stream_new(0x0129, 0x00fa);
	test_stream(ls);
	usbdc_stream_destroy(ls);
}
void test_stream(usbdc_stream *ls) {
	usbdc_message mess, mess2;
	mess.length = sprintf(mess.line_buf,
			"seee tyge45ypika grrr uyhtu6 5y6eatest dcdsh");
	mess.length = USB_MAX;
	usleep(1000 * 1000);
	while (1) {
		int ret = usbdc_stream_read_package(ls, 1, mess2.line_buf, USB_MAX);
		if (ret > 0)
			printf("mess is %.*s\nleng is :%d\n", ret, mess2.line_buf, ret);
		usbdc_stream_write(ls, 1, mess.line_buf, mess.length);
		usleep(10*1000);
	}
}
