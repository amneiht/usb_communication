/*
 * usb_handle.c
 *
 *  Created on: May 27, 2021
 *      Author: amneiht
 */

//void usbdc_init();
#include <usbdc.h>
#ifdef __linux__
#include <poll.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#define this "host_handle"

typedef struct {
	uint8_t act, in, out;
} epl;
int usbdc_loglv = 3;
static void write_complete(struct libusb_transfer *t) {
	usbdc_line *ls = t->user_data;
	usbdc_handle *han = ls->han;
	int state;
	switch (t->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		state = usbdc_state_commplete;
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
		ls->write_progess = usbdc_state_false;
		han->connect = 0;
		return;
	case LIBUSB_TRANSFER_CANCELLED:
		state = usbdc_state_false;
		puts("cancel tranfer");
		break;
	default:
		han->connect = 0;
		ls->write_progess = usbdc_state_false;
		return;
	}
	if (t == ls->write_data) {
		ls->head_request(ls->data, ls, state, 0);
	}

}
static void read_complete(struct libusb_transfer *t) {
	usbdc_line *ls = t->user_data;
	usbdc_handle *han = ls->han;
	int state;
	switch (t->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		state = usbdc_state_commplete;
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
		ls->read_progess = usbdc_state_false;
		han->connect = 0;
		return;
	case LIBUSB_TRANSFER_CANCELLED:
		/* This means that we are closing our program */
		state = usbdc_state_false;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		state = usbdc_state_false;
		break;
	default:
		han->connect = 0;
		ls->read_progess = usbdc_state_false;
		return;
	}
	if (t == ls->read_data) {
		ls->head_request(ls->data, ls, state, 1);
	}

}
static int create_line(usbdc_handle *han, epl *list, int lg) {
	usbdc_line **ls = han->line_array;
	han->nline = lg;
	for (int i = 0; i < lg; i++) {
		ls[i] = calloc(1, sizeof(usbdc_line));
		ls[i]->han = han;
		if (list[i].out != 0) {
			ls[i]->write_data = libusb_alloc_transfer(0);
			ls[i]->writebuff = malloc(sizeof(usbdc_message));
			libusb_fill_bulk_transfer(ls[i]->write_data, han->devh,
					list[i].out, (unsigned char*) &ls[i]->writebuff->length, 2,
					write_complete, ls[i], 0);
			ls[i]->write_data->user_data = ls[i];
		}
		if (list[i].in != 0) {
			ls[i]->read_data = libusb_alloc_transfer(0);
			ls[i]->readbuff = malloc(sizeof(usbdc_message));
			libusb_fill_bulk_transfer(ls[i]->read_data, han->devh, list[i].in,
					(unsigned char*) &ls[i]->readbuff->length,
					sizeof(usbdc_message), read_complete, ls[i], 0);
			ls[i]->read_data->user_data = ls[i];
		}
		ls[i]->write_progess = usbdc_state_false;
		ls[i]->read_progess = usbdc_state_false;
		usbdc_line_fast_mode(ls[i]);
	}
	return 0;
}
static int find_data_line(const struct libusb_interface_descriptor *idesc,
		epl *list) {
	for (int i = 0; i < idesc->bNumEndpoints; i++) {
		list[i].act = 0;
		list[i].in = 0;
		list[i].out = 0;
	}
	for (int i = 0; i < idesc->bNumEndpoints; i++) {
		uint8_t ep = idesc->endpoint[i].bEndpointAddress & 0x7f; // xoa bit dau
		for (int j = 0; j < idesc->bNumEndpoints; j++) {
			if (list[j].act == 0) {
				list[j].act = ep;
				if (idesc->endpoint[i].bEndpointAddress == list[j].act) {
					list[j].out = idesc->endpoint[i].bEndpointAddress;
				} else {
					list[j].in = idesc->endpoint[i].bEndpointAddress;
				}
				break;
			} else {
				if (list[j].act == ep) {
					if (idesc->endpoint[i].bEndpointAddress == list[j].act) {
						list[j].out = idesc->endpoint[i].bEndpointAddress;
					} else {
						list[j].in = idesc->endpoint[i].bEndpointAddress;
					}
					break;
				}
			}
		}
	}
	int ret = 0;
	for (int i = 0; i < idesc->bNumEndpoints; i++) {
		if (list[i].act != 0)
			ret++;
		else
			break;
	}
	return ret;
}
libusb_device_handle* find_device(libusb_context *ctx, uint16_t vendor_id,
		uint16_t product_id) {
	libusb_device **devices;
	libusb_device_handle *dvh = NULL;
	struct libusb_device_descriptor desc;
	int ndevices = libusb_get_device_list(ctx, &devices);
	if (ndevices < 0) {
		puts("Unable to initialize libusb");
		return NULL;
	}
	int ret;
	for (int i = 0; i < ndevices; i++) {

		ret = libusb_get_device_descriptor(devices[i], &desc);
		if (ret) {
			continue;
		}
		if (desc.idVendor == vendor_id || desc.idProduct == product_id) {
			ret = libusb_open(devices[i], &dvh);
			if (ret < 0) {
				puts("Unable to open device");
				dvh = NULL;
			} else
				break;
		}
	}
	libusb_free_device_list(devices, 1);
	return dvh;
}
usbdc_handle* usbdc_handle_new(uint16_t vendor_id, uint16_t product_id) {
	usbdc_handle *han = calloc(1, sizeof(usbdc_handle));
	int ret;
	ret = libusb_init(&han->ctx);
	if (ret) {
		usbdc_log(1, this, "Unable to initialize libusb");
		return NULL;
	}
	epl *list = NULL;
//todo this only for test . i must release it
//	libusb_device_handle *dvh = find_device(han->ctx, vendor_id, product_id);
	libusb_device_handle *dvh = libusb_open_device_with_vid_pid(han->ctx,
			vendor_id, product_id);
	if (dvh == NULL) {
		libusb_close(dvh);
		libusb_exit(han->ctx);
		//usbdc_log(3, this, "Unable to get device");
		goto out;
	}
	struct libusb_config_descriptor *desc;
	const struct libusb_interface_descriptor *idesc;
	libusb_device *dev = libusb_get_device(dvh);
	han->devh = dvh;
	ret = libusb_get_config_descriptor(dev, 0, &desc);
	if (ret < 0) {
		libusb_close(dvh);
		libusb_exit(han->ctx);
		usbdc_log(3, this, "Unable to get config desc");
		goto out;
	}
/// my lib not provice for more than 1 interface
	if (desc->bNumInterfaces != 1) {
		libusb_close(dvh);
		libusb_exit(han->ctx);
		usbdc_log(1, this, "this is only for one interface");
		goto out;
	}
	idesc = desc->interface[0].altsetting;
	list = malloc(idesc->bNumEndpoints * sizeof(epl));
	ret = find_data_line(idesc, list);
	if (ret < 0)
		usbdc_log(3, this, " nodata line");
	han->interface = idesc->bInterfaceNumber;
//dang ki interface
	int st = libusb_claim_interface(dvh, han->interface);
	if (st != 0) {
		libusb_close(dvh);
		libusb_exit(han->ctx);
		usbdc_log(1, this, "cannot claim interface");
		goto out;
	}
	han->line_array = calloc(ret, sizeof(usbdc_line*));
	st = create_line(han, list, ret);
//todo handle if fail
	han->connect = 1;
	free(list);
	return han;
	out: free(han);
	if (!list)
		free(list);
	return NULL;
}

void usbdc_handle_free(usbdc_handle *handle) {
	for (int i = 0; i < handle->nline; i++) {
//		usbdc_line_free(handle->line_array[i]);
		usbdc_line *line = handle->line_array[i];
		if (line->write_data != NULL) {
			libusb_cancel_transfer(line->write_data);

		}
		if (line->read_data != NULL) {
			libusb_cancel_transfer(line->read_data);

		}
	}
	struct timeval tv = { 0, 0 };
	libusb_handle_events_timeout_completed(handle->ctx, &tv, NULL);
	for (int i = 0; i < handle->nline; i++) {
		//		usbdc_line_free(handle->line_array[i]);
		usbdc_line *line = handle->line_array[i];
		if (line->write_data) {
			libusb_free_transfer(line->write_data);

			free(line->writebuff);
		}
		if (line->read_data) {
			libusb_free_transfer(line->read_data);
			free(line->readbuff);
		}
	}
	free(handle->line_array);
	libusb_release_interface(handle->devh, handle->interface);
	libusb_close(handle->devh);
	libusb_exit(handle->ctx);
	free(handle);
}
int usbdc_handle_checkevt(usbdc_handle *handle) {
	struct timeval time = { 0, 0 };
	return usbdc_handle_checkevt2(handle, &time);
}
int usbdc_handle_checkevt2(usbdc_handle *handle, struct timeval *tv) {

	usbdc_line *ls;
	int ret = libusb_handle_events_timeout_completed(handle->ctx, tv, NULL);
	if(ret<0)
	{
		usbdc_log(1, this, "Unable to use select");
		handle->connect = 0;
	}
	if (handle->connect) {
		for (int i = 0; i < handle->nline; i++) {
			ls = handle->line_array[i];
			if (ls->read_data) {
				if (ls->read_progess == usbdc_state_false) {
					ls->read_progess = usbdc_state_inprogess;
					gettimeofday(&handle->line_array[i]->tread, NULL);
					int ret = libusb_submit_transfer(ls->read_data);
					if (ret < 0) {
						usbdc_line_read_cancel(ls);
					}
				}
			}
		}
	}
	return 0;
}
