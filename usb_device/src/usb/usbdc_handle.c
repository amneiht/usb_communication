/*
 * usbdc_handle.c
 *
 *  Created on: May 26, 2021
 *      Author: amneiht
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <libaio.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <usb_dc.h>
#define this "usbdc_handle"
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define USB_TIME(a , b) \
 ((a.tv_sec - b.tv_sec)*1000 - (a.tv_usec - b.tv_usec)/1000)

#define timeout 2000
int usbdc_loglv = 1;

int usbdc_config(char *str, int slen, int **list, int nline);
static usbdc_handle* usbdc_handle_new(int ep0, int maxline) {
	usbdc_handle *han = calloc(1, sizeof(usbdc_handle));
	if (han == NULL) {
		usbdc_log(1, this, "can not malloc usb handle");
		return NULL;
	}
	han->ctx = NULL;
	int ret = io_setup(maxline * 4, &han->ctx);
	if (ret < 0) {
		usbdc_log(1, this, "can not set up io_ctx");
		goto freeh;

	}
	han->evt_fd = eventfd(0, 0);
	if (han->evt_fd < 0) {
		usbdc_log(1, this, "can not set open eventfd");
		goto freectx;
	}

	han->line_array = malloc(maxline * sizeof(usbdc_line));
	han->maxline = maxline;
	han->nline = 0;
	han->ep0 = ep0;
	return han;
	freectx: io_destroy(han->ctx);
	freeh: free(han);
	return NULL;

}
void usbdc_handle_free(usbdc_handle *handle) {
	free(handle->line_array);
	close(handle->evt_fd);
	close(handle->ep0);
	io_destroy(handle->ctx);
	free(handle);
}
int usbdc_handle_add_line(usbdc_handle *h, usbdc_line *line) {

	if (h->maxline == h->nline) {
		usbdc_log(2, this, "event handle over max size");
		return usbdc_error_eventover;
	}
	h->line_array[h->nline] = line;
	h->nline++;
	io_set_eventfd(&line->read_header, h->evt_fd);
	io_set_eventfd(&line->write_header, h->evt_fd);
	io_set_eventfd(&line->read_data, h->evt_fd);
	io_set_eventfd(&line->write_data, h->evt_fd);
	line->handle = h;
	return 0;
}
void handle_ep0(usbdc_handle *handle) {
	struct usb_functionfs_event event;
	int ret;
	ret = read(handle->ep0, &event, sizeof(event));
	if (!ret) {
//		report_error("unable to read event from ep0");
		usbdc_log(2, this, "unable to read event from ep0");
		return;
	}
	switch (event.type) {

	case FUNCTIONFS_SETUP:
		puts("setup funtion");
		/* stall for all setuo requests */
		if (event.u.setup.bRequestType & USB_DIR_IN)
			(void) write(handle->ep0, NULL, 0);
		else
			(void) read(handle->ep0, NULL, 0);
		break;

	case FUNCTIONFS_ENABLE:
		handle->connect = 1;
		fflush(stdout);
		/*
		 * Start receiving messages from host
		 *
		 * int recv_message()
		 */
		puts("enable funtion");
		break;
	case FUNCTIONFS_DISABLE:
		puts("disable funtion");
		for (int i = 0; i < handle->nline; i++) {
			usbdc_line_read_cancel(handle->line_array[i]);
			usbdc_line_write_cancel(handle->line_array[i]);
		}
		handle->connect = 0;
		break;
	default:
		break;
	}
}
void handle_events(usbdc_handle *handle) {
	uint64_t ev_cnt;
	struct io_event e[handle->nline * 2];
	int ret = read(handle->evt_fd, &ev_cnt, sizeof(ev_cnt));
	if (ret < 0) {
		usbdc_log(2, this, "unable to read eventfd");
		return;
	}
	struct timespec time = { 0, 0 };
	ret = io_getevents(handle->ctx, 1, ev_cnt, e, &time);
	if (ret < 0)
		return;
	for (int i = 0; i < ret; i++) {
		struct iocb *io = e[i].obj;
		if (io == NULL)
			continue;
		usbdc_line *dc = io->data;
		int res = e[i].res;
		if (res >= 0) {
			res = usbdc_state_commplete;
		} else {
			res = usbdc_state_false;
		}
		if (io == &dc->read_header) {
			dc->head_request(dc->data, dc, res, 1);
		} else if (io == &dc->write_header) {
			dc->head_request(dc->data, dc, res, 0);
		} else if (io == &dc->read_data) {
			dc->data_request(dc->data, dc, res, 1);
		} else if (io == &dc->write_data) {
			dc->data_request(dc->data, dc, res, 0);
		}
	}
}
void refresh_recv(usbdc_handle *handle) {

	for (int i = 0; i < handle->nline; i++) {
		if (handle->line_array[i]->read_progess == usbdc_state_false) {
			handle->line_array[i]->read_progess = usbdc_state_inprogess;
			gettimeofday(&handle->line_array[i]->tread, NULL);
			struct iocb *ios = &handle->line_array[i]->read_header;
			int ret = io_submit(handle->ctx, 1, &ios);
			if (ret < 0) {
				handle->line_array[i]->read_progess = usbdc_state_false;
			}
		}

	}
}
int usbdc_handle_checkevt(usbdc_handle *handle) {
	FD_ZERO(&handle->rfd);
	FD_SET(handle->ep0, &handle->rfd);
	FD_SET(handle->evt_fd, &handle->rfd);
	struct timeval timel = { 0, 0 };
	int max = MAX(handle->ep0, handle->evt_fd);
	int st = select(max + 1, &handle->rfd, NULL, NULL, &timel);
	if (st < 0) {
		return -1;
	} else if (st == 0) {
		goto out;
	}
	if (FD_ISSET(handle->ep0, &handle->rfd)) {
		handle_ep0(handle);
	}
	if (FD_ISSET(handle->evt_fd, &handle->rfd)) {
		handle_events(handle);
	}
	out: if (handle->connect)
		refresh_recv(handle);
	return 0;
}
usbdc_handle* usbdc_handle_create(char *ffs, int ffslen, int nline) {
	int *list;
	int list_len = usbdc_config(ffs, ffslen, &list, nline);
	if (list_len % 2 == 0) {
		usbdc_log(2, "usb_handle", "number of ep_in must equal ep_out");
		return NULL;
	}
	usbdc_handle *hs = usbdc_handle_new(list[0], nline);
	for (int i = 0; i < nline; i++) {
		usbdc_line *line = usbdc_line_new(list[2 * i + 1], list[2 * i + 2]);
		line->read_progess = usbdc_state_false;
		line->write_progess = usbdc_state_false;
		if (line == NULL) {
			goto freeh;
		}
		usbdc_handle_add_line(hs, line);
	}
	return hs;
	freeh: usbdc_handle_free(hs);
	usbdc_log(1, "usbdc_hander", "create failse");
	return NULL;
}
