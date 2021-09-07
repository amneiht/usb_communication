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
void test_stack() {

	usbdc_stack *bf = usbdc_stack_new(10, 2000);
	char mess[2000];
	char mess2[2000];
	sprintf(mess, "pikakakakak");
	usbdc_stack_push(bf, mess, 1999);
	usbdc_stack_push(bf, mess, 1999);
	usbdc_stack_pop(bf, mess2, 2000);
}
int main(int argc, char **argv) {
//	test_stack();
	usbdc_stream *ls = usbdc_stream_new(0x0129, 0x00fa);
	test_stream(ls);
	usbdc_stream_destroy(ls);

}
void test_stream(usbdc_stream *ls) {
	usbdc_message mess, mess2;
	mess2.length = sprintf(mess2.line_buf,
			"seee tyge45ypika grrr uyhtu6 5y6eatest dcdsh");
	usleep(1000 * 1000);
	while (1) {
		int ret = usbdc_stream_read_package(ls, 1, mess.line_buf, USB_MAX);
		if (ret > 0)
			printf("mess is %.*s\nleng is :%d\n", ret, mess.line_buf, ret);
		usbdc_stream_write(ls, 1, mess2.line_buf, mess2.length);
		usleep(1000 * 1000);
	}
}
