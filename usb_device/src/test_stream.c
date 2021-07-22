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
	mess2.length = sprintf(mess2.line_buf, "pika test dcdsh") + 2;
	while (1) {
		int ret = usbdc_stream_read_mess(ls, 1, &mess);
		usbdc_stream_write_mess(ls, 1, &mess2);
		if (ret > 0)
			printf("leng is :%d\n\n", mess.length);
		usleep(200 * 1000);

	}
}

