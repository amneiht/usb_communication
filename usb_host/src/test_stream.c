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
//	usbdc_buff *bf = usbdc_buff_new(3 * USB_MAX);
//	usbdc_message mess, mess2;
//	sprintf(mess.line_buf, "pika test dcdsh");
//	mess.length = strlen(mess.line_buf) + 2;
//
//	for (int i = 0; i < 1000; i++) {
//		usbdc_buff_push(bf, &mess, mess.length);
//	}
//	for (int i = 0; i < 1000; i++) {
//		usbdc_buff_pop_mess(bf, &mess2);
//		printf("mess is %.*s\nleng is :%d\n", mess2.length - 2, mess2.line_buf,
//				mess2.length);
//	}
	usbdc_stream *ls = usbdc_stream_new(0x0129, 0x00fa);
	test_stream(ls);
	usbdc_stream_destroy(ls);
}
void test_stream(usbdc_stream *ls) {
	usbdc_message mess;
	sprintf(mess.line_buf, "pika test dcdsh");
	mess.length = strlen(mess.line_buf) + 2;
	while (1) {
		usbdc_stream_write_mess(ls, 0, &mess);
		usleep(20 * 1000);
	}
}
