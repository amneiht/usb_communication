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
		struct iocb *ios = &line->read_header;
		// todo
		/*
		 * block read if data stack is full
		 */
		int ret = io_submit(line->handle->ctx, 1, &ios);
		if (ret < 0)
			line->read_progess = usbdc_state_false;
		return ret;

	} else {
		line->write_progess = state;
	}
	return 0;
}

//test ok
usbdc_stream* usbdc_stream_new(usbdc_handle *handle) {
	unsigned int maxstream = handle->nline;
	usbdc_stream *stream = calloc(1, sizeof(usbdc_stream));
	stream->run = 1;
	stream->handle = handle;
	stream->buff = malloc(maxstream * sizeof(usbdc_io_buff*));
	for (unsigned int i = 0; i < maxstream; i++) {
		stream->buff[i] = malloc(sizeof(usbdc_io_buff));
		stream->buff[i]->read = usbdc_stack_new(50, USB_MAX);
		stream->buff[i]->write = usbdc_stack_new(50, USB_MAX);
		pthread_mutex_init(&stream->buff[i]->pread, PTHREAD_MUTEX_NORMAL);
		pthread_mutex_init(&stream->buff[i]->pwrite, PTHREAD_MUTEX_NORMAL);
//		usbdc_line_read_cancel(handle->line_array[i]);
//		usbdc_line_write_cancel(handle->line_array[i]);
		handle->line_array[i]->data = stream->buff[i];
		handle->line_array[i]->head_request = &usb_io_header;
		handle->line_array[i]->read_header.u.c.nbytes = sizeof(usbdc_message);
	}
	pthread_create(&stream->thread, NULL, usbdc_run, stream);
	return stream;
}

static void* usbdc_run(void *arg) {
	usbdc_stream *stream = (usbdc_stream*) arg;
	int ret;
	struct timeval tm = { 1, 0 };
	while (stream->run) {
		tm.tv_sec = 1;
		tm.tv_usec = 0;
		usbdc_handle_checkevt2(stream->handle, &tm);
		//fixme : mtext add
		stream->connect = stream->handle->connect;
		if (stream->handle->connect) {
			for (unsigned int i = 0; i < stream->handle->nline; i++) {
				usbdc_io_buff *io = stream->buff[i];
				usbdc_line *line = stream->handle->line_array[i];
				pthread_mutex_lock(&io->pwrite);
				if (usbdc_line_is_write_ready(line)) {
					ret = usbdc_stack_package_pop(io->write,
							line->writebuff->line_buf, USB_MAX);
					line->writebuff->length = (uint16_t) (ret + 2);
					if (ret > 0) {
						line->write_header.u.c.nbytes = line->writebuff->length;
						line->writebuff->length = htole16(
								line->writebuff->length);
						line->write_progess = usbdc_state_inprogess;
						struct iocb *ios = &line->write_header;
						gettimeofday(&line->twrite, NULL);
						ret = io_submit(line->handle->ctx, 1, &ios);
						if (ret < 0)
							puts("usbdc_line send failse");
					}
				} else {
					if (usbdc_line_is_write_timeout(line, 2000)) {
						usbdc_stack_reset(io->write);
						usbdc_line_write_cancel(line);
					}

				}
				pthread_mutex_unlock(&io->pwrite);
			}
		} else {
			for (unsigned int i = 0; i < stream->handle->nline; i++) {
				usbdc_io_buff *io = stream->buff[i];
				usbdc_stack_reset(io->write);
				usbdc_line_write_cancel(stream->handle->line_array[i]);
				usbdc_line_read_cancel(stream->handle->line_array[i]);
			}
		}
	}
	for (unsigned int i = 0; i < stream->handle->nline; i++) {
		usbdc_io_buff *io = stream->buff[i];
		usbdc_stack_reset(io->write);
		usbdc_line_write_cancel(stream->handle->line_array[i]);
		usbdc_line_read_cancel(stream->handle->line_array[i]);
	}
	return NULL;
}
// test ok
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
/** write to mess
 * @fn int usbdc_stream_write(usbdc_stream*, int, int, void*)
 * @param stream
 * @param stream_id
 * @param slen
 * @param buff
 * @return 0 if write ok , -1 if no connect
 */
int usbdc_stream_write(usbdc_stream *stream, int _stream_id, void *buff,
		int slen) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_line *line = stream->handle->line_array[stream_id];
	pthread_mutex_lock(&io->pwrite);
	if (usbdc_line_is_write_ready(line)) {
		line->write_header.u.c.nbytes = (unsigned long) (slen + 2);
		ret = usbdc_line_write(line, buff, slen);
	} else {
		if (stream->handle->connect) {
			ret = usbdc_stack_push(io->write, buff, slen);
		}
	}
	pthread_mutex_unlock(&io->pwrite);
	return ret;
}
//[[deprecated("Replaced by bar, which has an improved interface")]]
int usbdc_stream_write_mess(usbdc_stream *stream, int _stream_id,
		usbdc_message *mess) {
	int ret = -1;
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_line *line = stream->handle->line_array[stream_id];
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pwrite);
	if (usbdc_line_is_write_ready(line)) {
		line->write_header.u.c.nbytes = mess->length;
		ret = usbdc_line_write(line, mess->line_buf, mess->length - 2);
	} else {
		if (stream->handle->connect) {
			ret = usbdc_stack_push(io->write, mess->line_buf, mess->length - 2);
		}
	}
	pthread_mutex_unlock(&io->pwrite);
	return ret;
}
int usbdc_stream_read_mess(usbdc_stream *stream, int _stream_id,
		usbdc_message *mess) {
	int ret = -1;
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	mess->length = 0;
	usbdc_io_buff *io = stream->buff[stream_id];
	ret = usbdc_stack_package_pop(io->read, mess->line_buf, USB_MAX);
	mess->length = (uint16_t) (ret + 2);
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
int usbdc_stream_read(usbdc_stream *stream, int _stream_id, void *buff,
		int slen) {
	int ret = -1;
	if (!stream->connect)
		return ret;
	unsigned int stream_id = (unsigned int) _stream_id;
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
int usbdc_stream_read_package(usbdc_stream *stream, int _stream_id, void *buff,
		int slen) {
	int ret = -1;
	if (!stream->connect) {
		puts("no connect");
		return ret;
	}
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline) {
		puts("over handle");
		return -1;
	}
	ret = -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	ret = usbdc_stack_package_pop(io->read, buff, slen);
	return ret;
}
void usbdc_stream_destroy(usbdc_stream *stream) {
	stream->run = 0;
	pthread_join(stream->thread, NULL);
	for (unsigned int i = 0; i < stream->handle->nline; i++) {
		usbdc_io_buff *io = stream->buff[i];
		pthread_mutex_destroy(&io->pread);
		pthread_mutex_destroy(&io->pwrite);
		usbdc_stack_free(io->write);
		usbdc_stack_free(io->read);
		free(io);
	}
	free(stream->buff);
	usbdc_handle_free(stream->handle);
	free(stream);
}
int usbdc_stream_write_clean(usbdc_stream *stream, int _stream_id) {
	int ret = -1;

	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pwrite);
	usbdc_stack_reset(io->write);
	pthread_mutex_unlock(&io->pwrite);
	return ret;
}
int usbdc_stream_read_clean(usbdc_stream *stream, int _stream_id) {
	int ret = -1;
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	pthread_mutex_lock(&io->pread);
	usbdc_stack_reset(io->read);
	pthread_mutex_unlock(&io->pread);
	return ret;
}
int usbdc_stream_clean(usbdc_stream *stream, int _stream_id) {
	int ret = -1;
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return -1;
	usbdc_io_buff *io = stream->buff[stream_id];
	usbdc_stack_reset(io->read);
	usbdc_stack_reset(io->write);
	return ret;
}
void usbdc_stream_print_info(usbdc_stream *stream, int _stream_id) {
	unsigned int stream_id = (unsigned int) _stream_id;
	if (stream_id >= stream->handle->nline)
		return;
	usbdc_io_buff *io = stream->buff[stream_id];
	printf("\n\n-------stream :%d--------\n", stream_id);
	printf("read stack\n");
	usbdc_stack_print_info(io->read);
	printf("write stack\n");
	usbdc_stack_print_info(io->write);
	printf("--------------------\n\n");
}
