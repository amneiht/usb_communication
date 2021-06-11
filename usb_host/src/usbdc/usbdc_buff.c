/*
 * usbdc_buff.c
 *
 *  Created on: Jun 2, 2021
 *      Author: amneiht
 */

#include <usb_dc.h>
#include <stdlib.h>
#include <string.h>

typedef struct usbdc_buff_p {
	struct usbdc_buff_p *next, *pre;
	int write, read;
	char *data;
} usbdc_buff_p;

struct usbdc_buff {
	int max_size;
	usbdc_buff_p *list;
};
static usbdc_buff_p* new_list(int max) {
	usbdc_buff_p *list = malloc(sizeof(usbdc_buff_p));
	if (!list)
		return NULL;
	list->read = 0;
	list->data = malloc(max * sizeof(char));
	list->write = 0;
	return list;
}
usbdc_buff* usbdc_buff_new(int max) {
	usbdc_buff *buff = malloc(sizeof(usbdc_buff));
	if (!buff)
		return NULL;
	buff->max_size = max;
	buff->list = new_list(max);
	buff->list->pre = buff->list;
	buff->list->next = buff->list;
	return buff;
}
void usbdc_buff_free(usbdc_buff *buff) {
	usbdc_buff_p *ls = buff->list->next;
	usbdc_buff_p *ps;
	while (ls != buff->list) {
		ps = ls->next;
		free(ls->data);
		free(ls);
		ls = ps;
	}
	free(ls->data);
	free(ls);
	free(buff);
}
static usbdc_buff_p* free_list(usbdc_buff_p *list) {
	usbdc_buff_p *next, *pre;
	next = list->next;
	pre = list->pre;
	if (pre == list) {
		list->write = 0;
		list->read = 0;
		return list;
	}
	next->pre = pre;
	pre->next = next;
	free(list->data);
	free(list);
	return next;
}
int usbdc_buff_pop(usbdc_buff *buff, char *data, int slen) {
	usbdc_buff_p *ls; // = buff->list;
	int ret = 0, lg, cps, make;
	do {
		make = 0;
		ls = buff->list;
		lg = ls->write - ls->read;
		if (lg < slen) {
			cps = lg;
			make = 1;
		} else {
			cps = slen;
		}
		ret = ret + cps;
		memcpy(data, buff->list->data, cps);
		data = data + cps;
		slen = slen - cps;
		ls->read = ls->read + cps;
		if (make) {
			buff->list = free_list(ls);
		}
		if (slen < 1)
			break;
	} while (buff->list->write != buff->list->read);
	return ret;

}
int usbdc_buff_push(usbdc_buff *buff, char *data, int slen) {
	usbdc_buff_p *ls = buff->list->pre;
	int lg, cps, make;
	while (slen > 0) {
		make = 0;
		lg = buff->max_size - ls->write; // so phan tu co the ghi
		if (slen <= lg)
			cps = slen;
		else {
			cps = lg;
			make = 1;
		}
		memcpy(ls->data + ls->write, data, cps);
		data = data + cps;
		ls->write = ls->write + cps;
		slen = slen - cps;
		if (make) {
			usbdc_buff_p *ps = new_list(buff->max_size);
			if (!ps)
				return -1;
			// chen them phan tu
			ls->next = ps;
			ps->pre = ls;
			buff->list->pre = ps;
			ps->next = buff->list;
			ls = ps;
		}
	}
	return 0;
}
