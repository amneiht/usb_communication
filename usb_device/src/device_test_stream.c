/*
 * test_stream.c
 *
 *  Created on: Jun 28, 2021
 *      Author: amneiht
 */

/*
 * test_stream.c
 *
 *  Created on: Jun 28, 2021
 *      Author: amneiht
 */

#include <usbdc_stream.h>
#include <unistd.h>
#include <stdio.h>
void test_stream(usbdc_stream *ls);
int main(int argc, char **argv) {
	usbdc_handle *han = usbdc_handle_create("/dev/ffs", 8, 2);
	usbdc_stream *ls = usbdc_stream_new(han);
	test_stream(ls);
	usbdc_stream_destroy(ls);
}
void test_stream(usbdc_stream *ls) {
	usbdc_message mess, mess2;
	mess2.length = (uint16_t) sprintf(mess2.line_buf, "pika test dcdsh");
	usleep(200 * 1000);
	while (1) {
		int ret = usbdc_stream_read_package(ls, 1, mess.line_buf, USB_MAX);
		if (ret > 0)
			printf("mess is %.*s\nleng is :%d\n", ret, mess.line_buf, ret);
//		else
//			puts("error");
		usbdc_stream_write(ls, 1, mess2.line_buf, 320);
		usleep(1000 * 1000);
	}
}

