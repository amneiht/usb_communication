/*
 * usb_line.c
 *
 *  Created on: May 25, 2021
 *      Author: amneiht
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#include <usbdc.h>

//static int com_header_request(void *data, usbdc_line *line, int state, int rw);

static int fast_header_request(void *data, usbdc_line *line, int state, int rw) {
	// read = 1 , write = 0
	(void) data;
	if (rw) {

		if (state == usbdc_state_false) {
			struct iocb *ios = &line->read_header;
			int ret = io_submit(line->handle->ctx, 1, &ios);
			if (ret < 0)
				line->read_progess = usbdc_state_false;
			return ret;

		} else {
			line->read_progess = usbdc_state_commplete;
			return 0;
		}
	} else {

		line->write_progess = state;
	}
	return 0;
}
void usbdc_line_fast_mode(usbdc_line *line) {
	line->read_header.u.c.nbytes = htole16(sizeof(usbdc_message));
	line->write_header.u.c.nbytes = htole16(sizeof(usbdc_message));
	line->head_request = fast_header_request;
}

/** CREATE NEW DATA LINE WITH ENNPOINT
 * @fn usbdc_line usbdc_new_line*(int, int)
 * @param ep_in	 usb in endpoint file decriptor
 * @param ep_out usb out endpoint file decriptor
 * @return usbdc_line if done , NULL if faile
 */

usbdc_line* usbdc_line_new(int ep_in, int ep_out) {
	usbdc_line *line = calloc(1, sizeof(usbdc_line));
	if (line == NULL) {
		puts("Cannot malloc new line");
		return NULL;
	}
	line->writebuff = malloc(sizeof(usbdc_message));
	if (line->writebuff == NULL) {
		puts("Cannot malloc data buff");
		goto freel;
	}
	io_prep_pwrite(&line->write_header, ep_in,
			(unsigned char*) &line->writebuff->length, 2, 0);
	line->readbuff = malloc(sizeof(usbdc_message));
	if (line->readbuff == NULL) {
		puts("Cannot malloc data buff");
		goto freew;
	}
	io_prep_pread(&line->read_header, ep_out,
			(unsigned char*) &line->readbuff->length, sizeof(usbdc_message), 0);
	line->write_header.data = line;
	line->read_header.data = line;
	line->read_progess = usbdc_state_false;
	line->write_progess = usbdc_state_false;
	line->head_request = &fast_header_request;
	return line;

	freew: free(line->writebuff);
	freel: free(line);
	return NULL;
}
int usbdc_line_is_write_ready(usbdc_line *line) {
	if (!line->handle->connect)
		return 0;
	if (line->write_progess == usbdc_state_inprogess
			|| line->write_progess == usbdc_state_handle)
		return 0;
	return 1;
}
int usbdc_line_is_read_ready(usbdc_line *line) {
	if (!line->handle->connect)
		return 0;
	if (line->read_progess == usbdc_state_commplete)
		return 1;
	return 0;
}
int usbdc_line_write_cancel(usbdc_line *line) {

	struct io_event e;
	if (line->handle->ctx == NULL) {
		puts("you for get register line to io context");
		return -1;
	}
	if (line->write_progess == usbdc_state_inprogess)
		io_cancel(line->handle->ctx, &line->write_header, &e);
	return 0;
}
int usbdc_line_read_cancel(usbdc_line *line) {
	if (!line->handle->connect)
		return 0;
	struct io_event e;
	if (line->handle->ctx == NULL) {
		puts("you for get register line to io context");
		return -1;
	}
	io_cancel(line->handle->ctx, &line->read_header, &e);
//	io_cancel(line->handle->ctx, &line->read_data, &e);
	return 0;
}
int usbdc_line_write(usbdc_line *line, char *buff, int slen) {
	if (!line->handle->connect)
		return -1;
	if (!line->handle->connect)
		return -1;
	if (line->handle->ctx == NULL) {
		puts("you for get register line to io context");
		return usbdc_error_no_line_ctx;
	}

	if (slen > USB_MAX) {
		puts("you can not send data is lagre than max transfer");
		return usbdc_error_sendover;
	}
	//todo chinh lai data
	line->write_progess = usbdc_state_inprogess;
	memcpy(line->writebuff->line_buf, buff, (unsigned int) slen);

	line->writebuff->length = htole16((uint16_t ) (slen + 2));
	struct iocb *ios = &line->write_header;
	gettimeofday(&line->twrite, NULL);
	int ret = io_submit(line->handle->ctx, 1, &ios);
	if (ret < 0)
		puts("usbdc_line send failse");
	return 0;
}
int usbdc_line_read(usbdc_line *line, char *buff, int slen) {
	if (!line->handle->connect)
		return 0;
	if (line->handle->ctx == NULL) {
		puts("you for get register line to io context");
		return usbdc_error_no_line_ctx;
	}
	if (slen < USB_MAX) {
		usbdc_log(2, "usb_line", "you should read USB_MAX data");
	}

	int sread = le16toh(line->readbuff->length) - 2;
	if (sread > slen)
		sread = slen;
	memcpy(buff, line->readbuff->line_buf, (unsigned int) sread);
	line->read_progess = usbdc_state_inprogess;
	gettimeofday(&line->tread, NULL);
	struct iocb *ios = &line->read_header;
	io_submit(line->handle->ctx, 1, &ios);
	return sread;
}
void usbdc_line_free(usbdc_line *line) {
	free(line->readbuff);
	free(line->writebuff);
	free(line);
}
