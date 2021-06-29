/*
 * usbdc_stream.c
 *
 *  Created on: Jun 28, 2021
 *      Author: amneiht
 */

#include <usbdc_stream.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
static void* usbdc_run(void *arg);
int create_io_buff(usbdc_stream *stream);

static int usb_io_header(void *data, usbdc_line *line, int state, int rw) {
	if (rw) {
		if (state == usbdc_state_commplete) {
			usbdc_io_buff *sbuff = data;
			line->readbuff->length = le16toh(line->readbuff->length);
			usbdc_buff_push(sbuff->read, line->readbuff,
					line->readbuff->length);
		}
		line->read_progess = usbdc_state_inprogess;
		int ret = libusb_submit_transfer(line->read_header);
		if (ret < 0)
			line->read_progess = usbdc_state_false;
		return ret;

	} else {
		line->write_progess = state;
	}
	return 0;
}
usbdc_stream* usbdc_stream_new(uint16_t vendor_id, uint16_t product_id) {

	usbdc_stream *stream = malloc(sizeof(usbdc_stream));
	stream->idProduct = product_id;
	stream->idVendor = vendor_id;
	stream->run = 1;
	stream->buff = malloc(32 * sizeof(usbdc_io_buff*));
	pthread_create(&stream->thread, NULL, &usbdc_run, stream);
	return stream;
}
int create_io_buff(usbdc_stream *stream) {
	int leng = stream->n_buff;
	if (leng < stream->handle->nline) {
		for (int i = leng; i < stream->handle->nline; i++) {
			stream->buff[i] = malloc(sizeof(usbdc_io_buff));
			stream->buff[i]->read = usbdc_buff_new(3 * USB_MAX);
			stream->buff[i]->write = usbdc_buff_new(3 * USB_MAX);
			pthread_mutex_init(&stream->buff[i]->pread, PTHREAD_MUTEX_NORMAL);
			pthread_mutex_init(&stream->buff[i]->pwrite, PTHREAD_MUTEX_NORMAL);
		}
		stream->n_buff = stream->handle->nline;
	}
	usbdc_handle *handle = stream->handle;
	for (int i = 0; i < stream->handle->nline; i++) {
		handle->line_array[i]->data = stream->buff[i];
		handle->line_array[i]->head_request = usb_io_header;
		handle->line_array[i]->read_header->length = sizeof(usbdc_message);
	}
	return stream->n_buff;

}
static void* usbdc_run(void *arg) {
	usbdc_stream *stream = (usbdc_stream*) arg;
	stream->connect = 0;
	stream->handle = NULL;
	stream->n_buff = 0;
	struct timeval tm = { 1, 0 };
	while (stream->run) {
		if (stream->handle) {
			usbdc_handle_free(stream->handle);
			stream->handle = NULL;
		}
		while (!stream->handle && stream->run) {
			stream->handle = usbdc_handle_new(stream->idVendor,
					stream->idProduct);
			usleep(100 * 1000);
		}
		if (!stream->run)
			break;
		stream->connect = 1;
		create_io_buff(stream);
		while (stream->run && stream->handle->connect) {
			usbdc_handle_checkevt2(stream->handle, &tm);
			if (stream->handle->connect) {
				for (int i = 0; i < stream->handle->nline; i++) {
					usbdc_io_buff *io = stream->buff[i];
					usbdc_line *line = stream->handle->line_array[i];
					if (usbdc_line_is_write_ready(line)) {
						int ret = usbdc_buff_pop_mess(io->write,
								line->writebuff);
						if (ret > 0) {
							line->write_header->length =
									line->writebuff->length;
							printf("write leng is :%d\n",line->write_header->length);
							line->writebuff->length = libusb_cpu_to_le16(
									line->writebuff->length);
							line->write_progess = usbdc_state_inprogess;
							gettimeofday(&line->twrite, NULL);
							int ret = libusb_submit_transfer(
									line->write_header);
							if (ret < 0)
								puts("usbdc_line send failse");
						}
					}
				}
			} else {
				stream->connect = 0;
				for (int i = 0; i < stream->handle->nline; i++) {
					usbdc_io_buff *io = stream->buff[i];
					pthread_mutex_lock(&io->pwrite);
					usbdc_buff_reset(io->write);
					pthread_mutex_unlock(&io->pwrite);
					usbdc_line_write_cancel(stream->handle->line_array[i]);
					usbdc_line_read_cancel(stream->handle->line_array[i]);
				}
			}
		}
		if (!stream->run)
			break;
		stream->connect = 0;
	}
	for (int i = 0; i < stream->handle->nline; i++) {
		usbdc_io_buff *io = stream->buff[i];
		pthread_mutex_lock(&io->pwrite);
		usbdc_buff_reset(io->write);
		pthread_mutex_unlock(&io->pwrite);
		usbdc_line_write_cancel(stream->handle->line_array[i]);
		usbdc_line_read_cancel(stream->handle->line_array[i]);
	}
	return NULL;
}

int usbdc_stream_write_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_line *line = stream->handle->line_array[stream_id];
	if (usbdc_line_is_write_ready(line)) {
		line->write_header->length = mess->length;
		ret = usbdc_line_write(line, mess->line_buf, mess->length - 2);
	} else {
		if (stream->handle->connect) {
			usbdc_io_buff *io = stream->buff[stream_id];
			pthread_mutex_lock(&io->pwrite);
			ret = usbdc_buff_push_mess(io->write, mess);
			pthread_mutex_unlock(&io->pwrite);
		}
	}
	return ret;
}
int usbdc_stream_read_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	if (stream_id >= stream->handle->nline)
		return -1;
	mess->length = -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pread);
	ret = usbdc_buff_pop_mess(io->read, mess);
	pthread_mutex_unlock(&io->pread);
	return ret;
}
void usbdc_stream_destroy(usbdc_stream *stream) {
	stream->run = 0;
	pthread_join(stream->thread, NULL);
	for (int i = 0; i < stream->handle->nline; i++) {
		usbdc_io_buff *io = stream->buff[i];
		pthread_mutex_destroy(&io->pread);
		pthread_mutex_destroy(&io->pwrite);
		usbdc_buff_free(io->write);
		usbdc_buff_free(io->read);
		free(io);
	}
	free(stream->buff);
	usbdc_handle_free(stream->handle);
	free(stream);
}
