/*
 * usb_dc.h
 *
 *  Created on: May 25, 2021
 *      Author: amneiht
 */

#ifndef USBDC_H_
#define USBDC_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <libusb-1.0/libusb.h>
#define USB_MAX  2000
#define usbdc_log(lv,name,reason) \
	if(usbdc_loglv >= lv) \
	fprintf(stderr,"file : %.10s ... %s\n",name,reason)

typedef struct usbdc_stack usbdc_stack;

extern int usbdc_loglv;
typedef struct usbdc_line usbdc_line;
typedef int (commplele_request)(void *data, usbdc_line *line, int rw);
typedef struct {
	uint16_t length;
	char line_buf[USB_MAX];
} __attribute((packed)) usbdc_message;

typedef enum usbdc_state {
	usbdc_state_commplete, //
	usbdc_state_inprogess, //
	usbdc_state_handle, //
	usbdc_state_false
} usbdc_state;
typedef enum usbdc_error {
	usbdc_error_no_line_ctx = -1, //
	usbdc_error_sendover = -2, //
	usbdc_error_eventover = -3
} usbdc_error;
typedef struct usbdc_handle {
	libusb_context *ctx;
	libusb_device_handle *devh;
	char interface;
#ifdef __linux__
	fd_set rfd, wfd;
	const struct libusb_pollfd **pfds ;
#endif
	//  ------------------- line ----------------
	int connect; // usb connect status
	int nline; // so event dang chua
	usbdc_line **line_array;
} usbdc_handle;

struct usbdc_line {
	//todo : create buffer to handle better read action
	usbdc_handle *han;
	struct libusb_transfer *write_data;
	struct libusb_transfer *read_data;
	usbdc_message *readbuff, *writebuff;
	struct timeval tread, twrite;
	int write_progess;
	int read_progess;
	int (*head_request)(void *data, usbdc_line *line, int state, int rw);
//	int (*data_request)(void *data, usbdc_line *line, int state, int rw);
	void *data;
};

void usbdc_line_fast_mode(usbdc_line *line);

// 1 is ready . 0 is not
int usbdc_line_is_write_ready(usbdc_line *line);
int usbdc_line_is_read_ready(usbdc_line *line);

// read_write
int usbdc_line_write_cancel(usbdc_line *line);
int usbdc_line_read_cancel(usbdc_line *line);
int usbdc_line_write(usbdc_line *line, char *buff, int slen);
int usbdc_line_read(usbdc_line *line, char *buff, int slen);

int usbdc_line_is_write_timeout(usbdc_line *line, int timeout);
int usbdc_line_is_read_timeout(usbdc_line *line, int timeout);


usbdc_stack* usbdc_stack_new(int maxlock, int block_size);
void usbdc_stack_free(usbdc_stack *buff) ;
void usbdc_stack_reset(usbdc_stack *buff) ;
int usbdc_stack_is_full(usbdc_stack *stack) ;
int usbdc_stack_push(usbdc_stack *stack, void *buff, int slen);
int usbdc_stack_pop(usbdc_stack *stack, void *buff, int slen) ;
int usbdc_stack_package_pop(usbdc_stack *stack, void *buff, int slen) ;
// usb handle
usbdc_handle* usbdc_handle_new(uint16_t vendor_id, uint16_t product_id);
void usbdc_handle_free(usbdc_handle *handle);
int usbdc_handle_checkevt(usbdc_handle *handle);
int usbdc_handle_checkevt2(usbdc_handle *handle, struct timeval *tv);

#ifdef __cplusplus
}
#endif
#endif /* USBDC_H_ */
