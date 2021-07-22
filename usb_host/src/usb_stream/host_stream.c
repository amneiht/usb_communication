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

static int usb_io_data(void *data, usbdc_line *line, int state, int rw) {
	if (rw) {
		if (state == usbdc_state_commplete) {
			usbdc_io_buff *sbuff = data;
			line->readbuff->length = le16toh(line->readbuff->length);
			int z = 1;
			pthread_mutex_lock(&sbuff->pread);
			if (sbuff->cb) {
				sbuff->cb(sbuff->userdata, line->readbuff);
				z = 0;
			}
			pthread_mutex_unlock(&sbuff->pread);
			if (z) {
				usbdc_stack_push(sbuff->read, line->readbuff->line_buf,
						line->readbuff->length - 2);
			}
		}
		line->read_progess = usbdc_state_inprogess;
		int ret = libusb_submit_transfer(line->read_data);
		if (ret < 0)
			line->read_progess = usbdc_state_false;
		return ret;

	} else {
		line->write_progess = state;
	}
	return 0;
}
int usbdc_stream_add_recv_cb(usbdc_stream *stream, int stream_id,
		usbdc_stream_cv *cb, void *data) {
	if (stream_id < 0 || stream_id >= stream->n_buff)
		return -1;
	usbdc_io_buff *ios = stream->buff[stream_id];
	pthread_mutex_lock(&ios->pread);
	ios->cb = cb;
	ios->userdata = data;
	pthread_mutex_unlock(&ios->pread);
	return 0;
}
usbdc_stream* usbdc_stream_new(uint16_t vendor_id, uint16_t product_id) {

	usbdc_stream *stream = calloc(1, sizeof(usbdc_stream));
	stream->idProduct = product_id;
	stream->idVendor = vendor_id;
	stream->run = 1;
	stream->buff = malloc(32 * sizeof(usbdc_io_buff*));
	pthread_create(&stream->thread, NULL, &usbdc_run, stream);
	return stream;
}
int create_io_buff(usbdc_stream *stream) {
	printf("dddd\n");
	int leng = stream->n_buff;
	if (leng < stream->handle->nline) {
		for (int i = leng; i < stream->handle->nline; i++) {
			stream->buff[i] = malloc(sizeof(usbdc_io_buff));
			stream->buff[i]->cb = NULL;
			stream->buff[i]->userdata = NULL;
			stream->buff[i]->read = usbdc_stack_new(100, USB_MAX);
			stream->buff[i]->write = usbdc_stack_new(100, USB_MAX);
			pthread_mutex_init(&stream->buff[i]->pread, PTHREAD_MUTEX_NORMAL);
		}
		stream->n_buff = stream->handle->nline;
	}
	usbdc_handle *handle = stream->handle;
	for (int i = 0; i < stream->handle->nline; i++) {
		libusb_cancel_transfer(handle->line_array[i]->read_data);
		handle->line_array[i]->data = stream->buff[i];
		handle->line_array[i]->head_request = usb_io_data;
		handle->line_array[i]->read_data->length = sizeof(usbdc_message);
	}

	return stream->n_buff;

}

static void* usbdc_run(void *arg) {
	usbdc_stream *stream = (usbdc_stream*) arg;
	stream->connect = 0;
	stream->handle = NULL;
	stream->n_buff = 0;
	int ret;
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
						ret = usbdc_stack_package_pop(io->write,
								line->writebuff->line_buf, USB_MAX);
						line->writebuff->length = ret + 2;
						if (ret > 0) {
							line->write_data->length = line->writebuff->length;
//							printf("write leng is :%d\n",
//									line->write_data->length);
							line->writebuff->length = libusb_cpu_to_le16(
									line->writebuff->length);
							line->write_progess = usbdc_state_inprogess;
							gettimeofday(&line->twrite, NULL);
							int ret = libusb_submit_transfer(line->write_data);
							if (ret < 0)
								puts("usbdc_line send failse");
						}
					} else {
						if (usbdc_line_is_write_timeout(line, 2000)) {
							usbdc_stack_reset(io->write);
							usbdc_line_write_cancel(line);
						}
					}
				}
			} else {
				stream->connect = 0;
				for (int i = 0; i < stream->handle->nline; i++) {
					usbdc_io_buff *io = stream->buff[i];
					usbdc_stack_reset(io->write);
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
		usbdc_stack_reset(io->write);
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
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_line *line = stream->handle->line_array[stream_id];
	if (usbdc_line_is_write_ready(line)) {
		line->write_data->length = mess->length;
		printf("mess leng is :%d\n", mess->length - 2);
		ret = usbdc_line_write(line, mess->line_buf, mess->length - 2);
	} else {
		if (stream->handle->connect) {
			ret = usbdc_stack_push(io->write, mess->line_buf, mess->length - 2);
		}
	}
	return ret;
}
/** write to mess
 * @fn int usbdc_stream_write(usbdc_stream*, int, int, void*)
 * @param stream
 * @param stream_id
 * @param slen
 * @param buff
 * @return 0 if write ok , -1 if no connect
 */
int usbdc_stream_write(usbdc_stream *stream, int stream_id, void *buff,
		int slen) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_line *line = stream->handle->line_array[stream_id];
	if (usbdc_line_is_write_ready(line)) {
		line->write_data->length = slen + 2;
		ret = usbdc_line_write(line, buff, slen);
	} else {
		if (stream->handle->connect) {
			ret = usbdc_stack_push(io->write, buff, slen);
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
	ret = usbdc_stack_package_pop(io->read, mess->line_buf, USB_MAX);
	mess->length = ret + 2;
	return ret;
}
/** read packets , try to read full size
 * @fn int usbdc_stream_read_package(usbdc_stream*, int, int, void*)
 * @param stream
 * @param stream_id
 * @param slen . leng of buffer
 * @param buff
 * @return leng of data , -1 if no connect , 0 is no data
 */
int usbdc_stream_read(usbdc_stream *stream, int stream_id, void *buff, int slen) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	if (stream_id >= stream->handle->nline)
		return -1;
	ret = -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	ret = usbdc_stack_pop(io->read, buff, slen);
	return ret;
}
/** read packets 1 integrity
 * @fn int usbdc_stream_read_package(usbdc_stream*, int, int, void*)
 * @param stream
 * @param stream_id
 * @param slen . leng of buffer
 * @param buff
 * @return leng of data , -1 if no connect , 0 is no data
 */
int usbdc_stream_read_package(usbdc_stream *stream, int stream_id, void *buff,
		int slen) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	if (stream_id >= stream->handle->nline)
		return -1;
	ret = -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	ret = usbdc_stack_package_pop(io->read, buff, slen);
	return ret;
}
void usbdc_stream_destroy(usbdc_stream *stream) {
	stream->run = 0;
	pthread_join(stream->thread, NULL);
	for (int i = 0; i < stream->handle->nline; i++) {
		usbdc_io_buff *io = stream->buff[i];
		pthread_mutex_destroy(&io->pread);
		usbdc_stack_free(io->write);
		usbdc_stack_free(io->read);
		free(io);
	}
	free(stream->buff);
	usbdc_handle_free(stream->handle);
	free(stream);
}
int usbdc_stream_write_clean(usbdc_stream *stream, int stream_id) {
	int ret = -1;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_stack_reset(io->write);
	return ret;
}
int usbdc_stream_read_clean(usbdc_stream *stream, int stream_id) {
	int ret = 0;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_stack_reset(io->read);
	return ret;
}
int usbdc_stream_clean(usbdc_stream *stream, int stream_id) {
	int ret = -1;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_stack_reset(io->read);
	usbdc_stack_reset(io->write);
	return ret;
}
