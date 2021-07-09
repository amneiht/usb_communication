/*
 * usbdc_stream.c
 *
 *  Created on: Jun 28, 2021
 *      Author: amneiht
 */

#include <usbdc_stream.h>
#include <stdio.h>

static void* usbdc_run(void *arg);
static int usb_io_header(void *data, usbdc_line *line, int state, int rw) {
	if (rw) {
		if (state == usbdc_state_commplete) {
			usbdc_io_buff *sbuff = data;
			line->readbuff->length = le16toh(line->readbuff->length);
//			printf("recv buff leng is :%d\n", line->readbuff->length);
			int z = 1;
			pthread_mutex_lock(&sbuff->pread);
			if (sbuff->cb) {
				sbuff->cb(sbuff->userdata, line->readbuff);
				z = 0;
			}
			pthread_mutex_unlock(&sbuff->pread);
			if (z) {
				usbdc_buff_push(sbuff->read, line->readbuff,
						line->readbuff->length);
			}
		}
		line->read_progess = usbdc_state_inprogess;
		struct iocb *ios = &line->read_header;
		int ret = io_submit(line->handle->ctx, 1, &ios);
		if (ret < 0)
			line->read_progess = usbdc_state_false;
		return ret;

	} else {
		line->write_progess = state;
	}
	return 0;
}
usbdc_stream* usbdc_stream_new(usbdc_handle *handle) {
	int maxstream = handle->nline;

	usbdc_stream *stream = malloc(sizeof(usbdc_stream));
	stream->run = 1;
	stream->handle = handle;
	stream->buff = malloc(maxstream * sizeof(usbdc_io_buff*));
	for (int i = 0; i < maxstream; i++) {
		stream->buff[i] = malloc(sizeof(usbdc_io_buff));
		stream->buff[i]->read = usbdc_buff_new(3 * USB_MAX);
		stream->buff[i]->write = usbdc_buff_new(3 * USB_MAX);
		pthread_mutex_init(&stream->buff[i]->pread, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&stream->buff[i]->pwrite, PTHREAD_MUTEX_NORMAL);
		handle->line_array[i]->data = stream->buff[i];
		handle->line_array[i]->head_request = usb_io_header;
		handle->line_array[i]->read_header.u.c.nbytes = htole16(
				sizeof(usbdc_message));
	}
	pthread_create(&stream->thread, NULL, &usbdc_run, stream);
	return stream;
}

static void* usbdc_run(void *arg) {
	usbdc_stream *stream = (usbdc_stream*) arg;
	struct timeval tm = { 1, 0 };
	while (stream->run) {
		usbdc_handle_checkevt2(stream->handle, &tm);
		if (stream->handle->connect) {
			for (int i = 0; i < stream->handle->nline; i++) {
				usbdc_io_buff *io = stream->buff[i];
				usbdc_line *line = stream->handle->line_array[i];
				if (usbdc_line_is_write_ready(line)) {
					int ret = usbdc_buff_pop_mess(io->write, line->writebuff);
					if (ret > 0) {
						line->write_header.u.c.nbytes = line->writebuff->length;
						line->writebuff->length = htole16(
								line->writebuff->length);
						line->write_progess = usbdc_state_inprogess;
						struct iocb *ios = &line->write_header;
						gettimeofday(&line->twrite, NULL);
						int ret = io_submit(line->handle->ctx, 1, &ios);
						if (ret < 0)
							puts("usbdc_line send failse");
					}
				}
			}
		} else {
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
int usbdc_stream_write_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess) {
	int ret = -1;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_line *line = stream->handle->line_array[stream_id];
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pwrite);
	if (usbdc_line_is_write_ready(line)) {
		line->write_header.u.c.nbytes = mess->length;
		ret = usbdc_line_write(line, mess->line_buf, mess->length);
	} else {
		if (stream->handle->connect) {
			ret = usbdc_buff_push_mess(io->write, mess);
		}
	}
	pthread_mutex_unlock(&io->pwrite);
	return ret;
}
int usbdc_stream_read_mess(usbdc_stream *stream, int stream_id,
		usbdc_message *mess) {
	int ret = -1;
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
int usbdc_stream_write_clean(usbdc_stream *stream, int stream_id) {
	int ret = -1;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pwrite);
	ret = usbdc_buff_reset(io->write);
	pthread_mutex_unlock(&io->pwrite);
	return ret;
}
int usbdc_stream_read_clean(usbdc_stream *stream, int stream_id) {
	int ret = -1;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pread);
	ret = usbdc_buff_reset(io->read);
	pthread_mutex_unlock(&io->pread);
	return ret;
}
