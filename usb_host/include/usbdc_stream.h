/*
 * usbdc_device_stream.c
 *
 *  Created on: Jun 26, 2021
 *      Author: amneiht
 */

#ifndef USBDC_STREAM_H_
#define USBDC_STREAM_H_

#include <usb_dc.h>
#include <pthread.h>
typedef enum usbdc_io_state {
	usbdc_io_state_free, //
	usbdc_io_state_recv //
} usbdc_io_state;

typedef struct usbdc_io_buff usbdc_io_buff;
struct usbdc_io_buff {
	pthread_mutex_t pread; // kh
	pthread_mutex_t pwrite; // kh
	usbdc_buff *read;
	usbdc_buff *write;
};

typedef struct usbdc_stream {
	int run;
	int connect;
	int n_buff;
	usbdc_handle *handle;
	pthread_t thread;
	usbdc_io_buff **buff;
	uint16_t idProduct;
	uint16_t idVendor;
} usbdc_stream;

int usbdc_stream_write_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess);
int usbdc_stream_read_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess);
usbdc_stream* usbdc_stream_new(uint16_t vendor_id, uint16_t product_id);
void usbdc_stream_destroy(usbdc_stream *stream);

#endif /* USBDC_STREAM_H_ */
