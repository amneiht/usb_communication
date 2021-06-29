/*
 * usbdc_host_line.c
 *
 *  Created on: May 28, 2021
 *      Author: amneiht
 */
#include <usb_dc.h>
#include <stdio.h>
#include <string.h>
#define fname "host_line.c"
static int fast_header_request(void *data, usbdc_line *line, int state, int rw) {
	// read = 1 , write = 0
	if (rw) {
		if (state == usbdc_state_false) {
			int ret = libusb_submit_transfer(line->read_header);
			if (ret < 0) {
				line->read_progess = usbdc_state_false;
				if (ret == LIBUSB_ERROR_NO_DEVICE) {
					line->han->connect = 0;
				}
			}
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
static int com_header_request(void *data, usbdc_line *line, int state, int rw) {
	// read = 1 , write = 0
	if (rw) {
		if (state == usbdc_state_false) {
			line->read_progess = usbdc_state_inprogess;
			gettimeofday(&line->tread, NULL);

			int ret = libusb_submit_transfer(line->read_header);
			if (ret < 0) {
				line->read_progess = usbdc_state_false;
				if (ret == LIBUSB_ERROR_NO_DEVICE) {
					line->han->connect = 0;
				}
			}
			return ret;
		} else {
			line->read_progess = usbdc_state_handle;
			gettimeofday(&line->tread, NULL);
			int ls = libusb_le16_to_cpu(line->readbuff->length) - 2;
			if (ls < 1 || ls > USB_MAX) {
				line->read_progess = usbdc_state_false;
				return 0;
			}
			line->readbuff->length = libusb_cpu_to_le16(ls);
			line->read_data->length = ls;
			int ret = libusb_submit_transfer(line->read_data);
			if (ret < 0) {
				line->read_progess = usbdc_state_false;
				if (ret == LIBUSB_ERROR_NO_DEVICE) {
					line->han->connect = 0;
				}
			}
			return ret;
		}
	} else {
		if (state == usbdc_state_false) {
			line->write_progess = usbdc_state_false;
			return 0;
		} else {
			int ls = libusb_le16_to_cpu(line->writebuff->length) - 2;
			//todo test pipe
			line->write_data->length = ls;
			line->write_progess = usbdc_state_handle;
			if (ls < 1 || ls > USB_MAX) {
				line->write_progess = usbdc_state_false;
				return 0;
			}
			int ret = libusb_submit_transfer(line->write_data);
			if (ret < 0) {
				line->write_progess = usbdc_state_false;
				if (ret == LIBUSB_ERROR_NO_DEVICE) {
					line->han->connect = 0;
				}
			}
			return ret;
		}
	}
	return -1;
}
static int com_data_request(void *data, usbdc_line *line, int state, int rw) {
	// read = 1 , write = 0
	if (rw) {
		if (state == usbdc_state_false) {
			line->read_progess = usbdc_state_inprogess;
			gettimeofday(&line->tread, NULL);
			int ret = libusb_submit_transfer(line->read_header);
			if (ret < 0) {
				line->read_progess = usbdc_state_false;
				if (ret == LIBUSB_ERROR_NO_DEVICE)
					line->han->connect = 0;
			}
			return ret;
		} else {
			line->read_progess = usbdc_state_commplete;
		}
	} else {
		line->write_progess = state;
	}
	return 0;
}
void usbdc_line_fast_mode(usbdc_line *line) {
	if (line->read_header) {
		libusb_cancel_transfer(line->read_header);
		libusb_cancel_transfer(line->read_data);
		line->read_header->length = libusb_cpu_to_le16(sizeof(usbdc_message));
	}
	if (line->write_header) {
		libusb_cancel_transfer(line->write_header);
		libusb_cancel_transfer(line->write_data);
		line->write_header->length = libusb_cpu_to_le16(sizeof(usbdc_message));
	}
	line->head_request = &fast_header_request;
}
void usbdc_line_com_mode(usbdc_line *line) {
	if (line->read_header) {
		libusb_cancel_transfer(line->read_header);
		libusb_cancel_transfer(line->read_data);
		line->read_header->length = 2;
	}
	if (line->write_header) {
		libusb_cancel_transfer(line->write_header);
		libusb_cancel_transfer(line->write_data);
		line->write_header->length = 2;
	}
	line->head_request = &com_header_request;
	line->data_request = &com_data_request;
}
int usbdc_line_is_write_ready(usbdc_line *line) {
	if (line->write_header) {
		if (line->write_progess == usbdc_state_inprogess
				|| line->write_progess == usbdc_state_handle) {
			return 0;
		}
		return 1;
	} else {
		usbdc_log(2, fname, "you cannt send data on this line");
		return 0;
	}
}
int usbdc_line_is_read_ready(usbdc_line *line) {
	if (line->read_header) {
		if (line->read_progess == usbdc_state_commplete) {
			return 1;
		}
		return 0;
	} else {
		usbdc_log(2, fname, "you cannt read data on this line");
		return 0;
	}
}
int usbdc_line_write_cancel(usbdc_line *line) {
	if (line->write_header) {
		if (line->write_progess == usbdc_state_handle) {
			int ret = libusb_cancel_transfer(line->write_data);
			if (ret == LIBUSB_ERROR_NO_DEVICE) {
				line->han->connect = 0;
			}
		} else if (line->write_progess == usbdc_state_inprogess) {
			int ret = libusb_cancel_transfer(line->write_header);
			if (ret == LIBUSB_ERROR_NO_DEVICE) {
				line->han->connect = 0;
			}
		}
		return 0;
	}
	usbdc_log(2, fname, "no write tranfer to cancel");
	return -1;
}
int usbdc_line_read_cancel(usbdc_line *line) {

	if (line->read_header) {
		if (line->write_progess == usbdc_state_handle) {
			int ret = libusb_cancel_transfer(line->read_data);
			if (ret == LIBUSB_ERROR_NO_DEVICE) {
				line->han->connect = 0;
			}
		}
		return 0;
	}
	usbdc_log(2, fname, "no read tranfer to cancel");
	return -1;
}
int usbdc_line_write(usbdc_line *line, char *buff, int slen) {
	if (!line->han || !line->han->connect)
		return 0;
	if (line->write_header == NULL) {
		usbdc_log(2, fname, "no enpoint to sen data");
		return -10;
	}
	if (line->write_progess == usbdc_state_inprogess) {
		usbdc_log(2, fname, "cant not write: we are seding data");
		return LIBUSB_ERROR_BUSY;

	}
	if (slen <= USB_MAX) {
		memcpy(line->writebuff->line_buf, buff, slen);
		line->write_data->length = slen;
		line->writebuff->length = libusb_cpu_to_le16(slen + 2);
		line->write_progess = usbdc_state_inprogess;
		gettimeofday(&line->twrite, NULL);
		int ret = libusb_submit_transfer(line->write_header);
		if (ret < 0) {
			puts(libusb_error_name(ret));
			line->write_progess = usbdc_state_false;
			if (ret == LIBUSB_ERROR_NO_DEVICE) {
				line->han->connect = 0;
			}
		}
		return ret;
	}
	return LIBUSB_ERROR_OVERFLOW;
}
int usbdc_line_read(usbdc_line *line, char *buff, int slen) {
	if (!line->han || !line->han->connect)
		return 0;
	if (line->read_header == NULL) {
		usbdc_log(2, fname, "no enpoint to sen data");
		return -10;
	}
	if (line->read_progess != usbdc_state_commplete) {
		usbdc_log(2, fname, "cant not read: we are waite data");
		return LIBUSB_ERROR_BUSY;

	}

	int dlen = libusb_le16_to_cpu(line->readbuff->length) - 2;
	if (slen < dlen)
		dlen = slen;
	memcpy(buff, line->readbuff->line_buf, dlen);
	line->read_progess = usbdc_state_inprogess;
	gettimeofday(&line->tread, NULL);
	int ret = libusb_submit_transfer(line->read_header);
	if (ret < 0) {
		line->read_progess = usbdc_state_false;

		if (ret == LIBUSB_ERROR_NO_DEVICE) {
			line->han->connect = 0;
		}
	}
	return dlen;
}
