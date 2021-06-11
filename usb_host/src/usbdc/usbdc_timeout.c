/*
 * usbdc_timeout.c
 *
 *  Created on: Jun 2, 2021
 *      Author: amneiht
 */

#include <usb_dc.h>
#include <stdlib.h>
#include <stdio.h>
static long time_diff(struct timeval *a, struct timeval *b) {
	long res = ((a->tv_sec - b->tv_sec) * 1000
			- (a->tv_usec - b->tv_usec) / 1000);
	if (res < 0)
		res = -res;
	return res;
}
int usbdc_line_is_write_timeout(usbdc_line *line, int timeout) {
	if (!line->han || !line->han->connect)
		return 1;
	if (line->write_progess != usbdc_state_inprogess
			&& line->write_progess != usbdc_state_handle)
		return 0;
	struct timeval tm;
	gettimeofday(&tm, NULL);
	long res = time_diff(&tm, &line->twrite);
	res = res - timeout;
	if (res > 0)
		return 1;
	return 0;
}
int usbdc_line_is_read_timeout(usbdc_line *line, int timeout) {
	if (!line->han || !line->han->connect)
		return 1;
	if (line->read_progess == usbdc_state_commplete
			|| line->read_progess == usbdc_state_false)
		return 0;
	if (line->read_progess == usbdc_state_inprogess) {
		gettimeofday(&line->tread, NULL);
		line->read_progess = usbdc_state_handle;
		return 0;
	}
	struct timeval tm;
	gettimeofday(&tm, NULL);
	int res = time_diff(&tm, &line->tread) - timeout;
	if (res > 0)
		return 1;
	return 0;
}
