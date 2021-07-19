/*
 * usbdc_package.c
 *
 *  Created on: Jul 17, 2021
 *      Author: amneiht
 */

#include <usb_dc.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

typedef struct usbdc_stack_block usbdc_stack_block;
struct usbdc_stack {
	int block_size;
	pthread_mutex_t wlock;
	pthread_mutex_t rlock;
	usbdc_stack_block *tmp;
	usbdc_stack_block *pread, *pwrite;
	usbdc_stack_block *list;
};
struct usbdc_stack_block {
	int id;
	usbdc_stack_block *next;
	int write, read;
	char *buff;
};
static int is_free_block(usbdc_stack_block *ls) {
	return ls->write == 0 || (ls->read == ls->write);
}
static usbdc_stack_block* new_list(usbdc_stack_block *pre, int blocksize) {
	usbdc_stack_block *list = malloc(sizeof(usbdc_stack_block));
	if (!list)
		return NULL;
	list->read = 0;
	list->buff = malloc(sizeof(char) * (blocksize + 2));
	list->write = 0;
	if (pre != NULL) {
		list->id = pre->id + 1;
		list->next = pre->next;
		pre->next = list;
	} else {
		list->id = 0 + 1;
		list->next = list;
	}
	return list;
}
usbdc_stack* usbdc_stack_new(int maxlock, int block_size) {
	usbdc_stack *buff = malloc(sizeof(usbdc_stack));
	if (!buff)
		return NULL;
	buff->block_size = block_size;
	buff->list = new_list(NULL, block_size);
	usbdc_stack_block *ls = buff->list;
	if (maxlock < 10)
		maxlock = 10;
	for (int i = 1; i < maxlock; i++) {
		ls = new_list(ls, block_size);
	}
	buff->tmp = new_list(NULL, block_size);
	buff->tmp->read = 0;
	buff->tmp->write = 0;
	buff->pread = buff->list;
	buff->pwrite = buff->list;
	pthread_mutex_init(&buff->wlock, PTHREAD_MUTEX_NORMAL);
	pthread_mutex_init(&buff->rlock, PTHREAD_MUTEX_NORMAL);
	return buff;
}
void usbdc_stack_free(usbdc_stack *buff) {
	usbdc_stack_block *ls = buff->list->next;
	usbdc_stack_block *ps;
	while (ls != buff->list) {
		ps = ls->next;
		free(ls->buff);
		free(ls);
		ls = ps;
	}
	free(ls->buff);
	free(ls);
	free(buff->tmp->buff);
	free(buff->tmp);
	pthread_mutex_destroy(&buff->rlock);
	pthread_mutex_destroy(&buff->rlock);
	free(buff);
}
void usbdc_stack_reset(usbdc_stack *buff) {
	pthread_mutex_lock(&buff->wlock);
	pthread_mutex_lock(&buff->rlock);
	usbdc_stack_block *ls = buff->list;
	usbdc_stack_block *ps = ls->next;
	usbdc_stack_block *d;
	while (ls != ps) {
		d = ps->next;
		ps->read = 0;
		ps->write = 0;
		ps = d;
	}
	buff->tmp->read = 0;
	buff->tmp->write = 0;
	ls->read = 0;
	ls->write = 0;
	buff->pread = ls;
	buff->pwrite = ls;
	pthread_mutex_unlock(&buff->rlock);
	pthread_mutex_unlock(&buff->wlock);
}
static void reset_block(usbdc_stack_block *ls) {
	ls->read = 0;
	ls->write = 0;
}
static usbdc_stack_block* get_data(usbdc_stack_block *ls, void *data, int slen,
		int *ret, int blog_size) {
	int bsize = blog_size + 2;
	if (ls->write == 0 || (ls->read == ls->write)) {
		*ret = 0;
		reset_block(ls);
		return ls;
	}
	int cps, lg, bread = 0;
	void *get;
	do {
		lg = ls->write - ls->read;
		get = ls->buff + ls->read;
		if (lg > slen) {
			cps = slen;
			ls->read = ls->read + cps;
		} else {
			cps = lg;
			ls->read = ls->read + cps;
			if (ls->read == bsize) {
				reset_block(ls);
				ls = ls->next;
			} else if (ls->next->write > 0) {
				ls = ls->next;
			}
		}
		memcpy(data, get, cps);
		data = data + cps;
		slen = slen - cps;
		bread = bread + cps;
		if (slen < 1 || cps < 1)
			break;

	} while (ls->write != 0 && (ls->read != ls->write));
	*ret = bread;
	return ls;
}
int usbdc_stack_is_full(usbdc_stack *stack) {
	if (is_free_block(stack->pwrite)) {
		return 0;
	}
	if (is_free_block(stack->pwrite->next)) {
		return 0;
	}
	return 1;
}
static usbdc_stack_block* drop_package(usbdc_stack_block *ls, int bsize) {
	if (is_free_block(ls))
		return ls;
	uint16_t size = 0;
	int ret, lg;
	usbdc_stack_block *cmp = ls;
	do {
		ls = get_data(ls, &size, 2, &ret, bsize);
		if (ret < 2 || size == 0) {
			return ls;
		}

		while (size > 0) {
			lg = ls->write - ls->read;
			if (size < lg) {
				ls->read = ls->read + size;
				size = 0;
			} else {
				size = size - lg;
				reset_block(ls);
				ls = ls->next;
			}
		}
	} while (cmp == ls);
	return ls;
}
static usbdc_stack_block* write_data(usbdc_stack_block *ls, void *data,
		int slen, int max_block) {
	int bsize = max_block + 2;
	int cps, lg;
	void *pr;
	while (slen > 0) {
		lg = bsize - ls->write;
		pr = ls->buff + ls->write;
		if (lg < slen) {
			cps = lg;
			ls->write = ls->write + cps;
			ls = ls->next;
		} else {
			cps = slen;
			ls->write = ls->write + cps;
		}
		memcpy(pr, data, cps);
		slen = slen - cps;
		data = data + cps;
	}
	return ls;
}
/**put you data to stack , you data is not more than block_size you set
 * @fn int usbdc_stack_push(usbdc_stack*, void*, int)
 * @param stack
 * @param buff
 * @param slen
 * @return
 */
int usbdc_stack_push(usbdc_stack *stack, void *buff, int slen) {
	if (slen > stack->block_size)
		return -1;
	pthread_mutex_lock(&stack->wlock);
	if (stack->pwrite->next == stack->pread) {
		pthread_mutex_lock(&stack->rlock);
		stack->pread = drop_package(stack->pread, stack->block_size);
		pthread_mutex_unlock(&stack->rlock);
	}
	uint16_t size = slen;
	stack->pwrite = write_data(stack->pwrite, &size, 2, stack->block_size);
	stack->pwrite = write_data(stack->pwrite, buff, slen, stack->block_size);
	pthread_mutex_unlock(&stack->wlock);
	return 0;
}
int usbdc_stack_pop(usbdc_stack *stack, void *buff, int slen) {
	pthread_mutex_lock(&stack->rlock);
	int ret = 0, total;
	get_data(stack->tmp, buff, slen, &ret, stack->block_size);
	slen = slen - ret;
	if (slen < 1) {
		return ret;
	}
	buff = buff + ret;
	total = ret;
	uint16_t size;
	int cps, check = 0;
	do {
		// read mess leng
		size = 0;
		stack->pread = get_data(stack->pread, &size, 2, &ret,
				stack->block_size);
		if (ret < 2) {
			// no mess
			break;
		}
		if (size > slen) {
			cps = slen;
			check = size - slen;
		} else {
			cps = size;
		}
		stack->pread = get_data(stack->pread, buff, cps, &ret,
				stack->block_size);
		slen = slen - cps;
		buff = buff + cps;
		total = total + cps;
		if (slen < 1) {
			break;
		}
	} while ((stack->pread->write != 0)
			&& (stack->pread->write != stack->pread->read));
	if (check) {
		stack->pread = get_data(stack->pread, stack->tmp->buff, check, &ret,
				stack->block_size);
		stack->tmp->write = ret;
		stack->tmp->read = 0;
	}
	pthread_mutex_unlock(&stack->rlock);
	return total;
}
/**
 * get data with integrity
 * example : if you push p1 :8 byte ,p2: 6 byte ,p3:12 byte ;
 * if you pop with 20 byte buffer , you will get p1 , p2
 * @fn int usbdc_stack_package_pop(usbdc_stack*, void*, int)
 * @param stack
 * @param buff
 * @param slen
 * @return
 */
int usbdc_stack_package_pop(usbdc_stack *stack, void *buff, int slen) {
	pthread_mutex_lock(&stack->rlock);
	int ret = 0, total;
	if (stack->tmp->write - stack->tmp->read < slen)
		get_data(stack->tmp, buff, slen, &ret, stack->block_size);
	slen = slen - ret;
	if (slen < 1) {
		return ret;
	}
	buff = buff + ret;
	total = ret;
	uint16_t size;
	int cps, check = 0;
	do {
		// read mess leng
		size = 0;
		stack->pread = get_data(stack->pread, &size, 2, &ret,
				stack->block_size);
		if (ret < 2) {
			// no mess
			break;
		}
		if (size > slen) {
			check = size;
			break;
		} else {
			cps = size;
		}
		stack->pread = get_data(stack->pread, buff, cps, &ret,
				stack->block_size);
		slen = slen - cps;
		buff = buff + cps;
		total = total + cps;
		if (slen < 0) {
			break;
		}
	} while (stack->pread->write != 0
			&& (stack->pread->write != stack->pread->read));
	if (check) {
		stack->pread = get_data(stack->pread, stack->tmp->buff, check, &ret,
				stack->block_size);
		stack->tmp->write = ret;
		stack->tmp->read = 0;
	}
	pthread_mutex_unlock(&stack->rlock);
	return total;
}
