/*
 * usb_dc.h
 *
 *  Created on: May 25, 2021
 *      Author: amneiht
 */

#ifndef USB_DC_H_
#define USB_DC_H_

#include <libaio.h>
#include <stdint.h>
#include <sys/time.h>
#define USB_MAX  2000

#define usbdc_log(lv,name,reason) \
	if(usbdc_loglv >= lv) \
	fprintf(stderr,"file : %.10s ... %s\n",name,reason)

extern int usbdc_loglv;

typedef struct usbdc_line usbdc_line;
typedef struct usbdc_buff usbdc_buff;
typedef struct usbdc_handle usbdc_handle;
typedef struct {
	uint16_t length;
	char line_buf[USB_MAX];
} __attribute((packed)) usbdc_message;

typedef enum usbdc_state {
	usbdc_state_commplete, // hoan thanh moi yeu cau
	usbdc_state_handle, // dang cho hoan thanh yeu cau
	usbdc_state_inprogess, // dang xu ly yeu cau
	usbdc_state_false // loi
} usbdc_state;
typedef enum usbdc_error {
	usbdc_error_no_line_ctx = -1, //
	usbdc_error_sendover = -2, //
	usbdc_error_eventover = -3

} usbdc_error;
struct usbdc_line {
	//todo : create buffer to handle better read action
	usbdc_handle *handle;
	struct iocb write_header;
	struct iocb read_header;
	struct iocb write_data;
	struct iocb read_data;
	usbdc_message *readbuff, *writebuff;
	struct timeval tread, twrite;
	int write_progess;
	int read_progess;
	int (*head_request)(void *data, usbdc_line *line, int state, int rw);
	int (*data_request)(void *data, usbdc_line *line, int state, int rw);
	void *data;
};
struct usbdc_handle {
	io_context_t ctx;
	int ep0; // ep0 file decsripter
	int evt_fd; // io event handle
	//  ------------------- line ----------------
	int connect; // usb connect status
	fd_set rfd, wfd;
	int maxline; // max event
	int nline; // so event dang chua
	usbdc_line **line_array;
};

/// usb line funtion
usbdc_line* usbdc_line_new(int ep_in, int ep_out);
void usbdc_line_free(usbdc_line *line);

int usbdc_line_is_write_ready(usbdc_line *line);
int usbdc_line_is_read_ready(usbdc_line *line);
int usbdc_line_write_cancel(usbdc_line *line);
int usbdc_line_read_cancel(usbdc_line *line);
int usbdc_line_write(usbdc_line *line, char *buff, int slen);
int usbdc_line_read(usbdc_line *line, char *buff, int slen);
void usbdc_line_fast_mode(usbdc_line *line);
void usbdc_line_com_mode(usbdc_line *line);
// time out handle
int usbdc_line_is_write_timeout(usbdc_line *line, int timeout);
int usbdc_line_is_read_timeout(usbdc_line *line, int timeout);

//usbdc_buffer hepler
int usbdc_buff_push(usbdc_buff *buff, void *data, int slen);
int usbdc_buff_pop(usbdc_buff *buff, void *data, int slen);
usbdc_buff* usbdc_buff_new(int max);

int usbdc_buff_push_mess(usbdc_buff *buff, usbdc_message *mess);
int usbdc_buff_pop_mess(usbdc_buff *buff, usbdc_message *mess);

void usbdc_buff_reset(usbdc_buff *buff);
void usbdc_buff_free(usbdc_buff *buff);

//usb connect funtion
//usbdc_handle* usbdc_handle_new(int ep0, int maxline);

//
usbdc_handle* usbdc_handle_create(char *ffs, int ffslen, int nline);
void usbdc_handle_free(usbdc_handle *handle);
int usbdc_handle_checkevt(usbdc_handle *handle);
int usbdc_handle_checkevt2(usbdc_handle *handle, struct timeval *time);
int usbdc_handle_add_line(usbdc_handle *h, usbdc_line *line);

// usbdc buff

#endif /* USB_DC_H_ */
