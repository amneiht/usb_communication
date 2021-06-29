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
	usbdc_handle *han = usbdc_handle_create("/dev/ffs", 8, 1);
	usbdc_stream *ls = usbdc_stream_new(han);
	test_stream(ls);
	usbdc_stream_destroy(ls);
}
void test_stream(usbdc_stream *ls) {
	usbdc_message mess;
	while (1) {
		int ret = usbdc_stream_read_mess(ls, 0, &mess);
		if (ret > 0)
			printf("mess is %s\nleng is :%d\n", mess.line_buf, mess.length);

	}
}

