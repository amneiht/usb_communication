/*
 * usbdc_device_stream.c
 *
 *  Created on: Jun 26, 2021
 *      Author: amneiht
 */

#ifndef USBDC_STREAM_H_
#define USBDC_STREAM_H_
#ifdef __cplusplus
	extern "C" {
#endif
#include <usb_dc.h>
#include <pthread.h>

typedef struct usbdc_io_buff usbdc_io_buff;
typedef void (usbdc_stream_cv)(void *data, usbdc_message *mess);
struct usbdc_io_buff {
	pthread_mutex_t pread; // kh
	pthread_mutex_t pwrite; // kh
	usbdc_stream_cv *cb;
	void *userdata;
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
int usbdc_stream_add_recv_cb(usbdc_stream * stream ,int stream_id ,usbdc_stream_cv * cb , void * data) ;
int usbdc_stream_write_clean(usbdc_stream *stream, int stream_id);
int usbdc_stream_read_clean(usbdc_stream *stream, int stream_id);

#ifdef __cplusplus
	extern }
#endif

#endif /* USBDC_STREAM_H_ */
