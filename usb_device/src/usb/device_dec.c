#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <usbdc.h>
#define STR_INTERFACE "AMNEIHT Data line config"

struct usbdc_pre_deader {
	struct usb_functionfs_descs_head_v2 header;
	__le32 fs_count;
	__le32 hs_count;
} __attribute__ ((__packed__));

static int usbdc_ep_description(const int line, const char *str, int slen);

static const struct {
	struct usb_functionfs_strings_head header;
	struct {
		__le16 code;
		const char str1[sizeof(STR_INTERFACE)];
	} __attribute__ ((__packed__)) lang0;
} __attribute__ ((__packed__)) strings = { .header = { .magic = htole32(
		FUNCTIONFS_STRINGS_MAGIC), .length = htole32(sizeof(strings)),
		.str_count = htole32(1), .lang_count = htole32(1), }, .lang0 = {
		htole16(0x0409), /* en-us */
		STR_INTERFACE, }, };

static void ep_dec(struct usb_endpoint_descriptor_no_audio *ep,
		unsigned char addr, unsigned char type, short unsigned int size) {
	ep->bLength = sizeof(*ep);
	ep->bDescriptorType = USB_DT_ENDPOINT;
	ep->bEndpointAddress = addr | type;
	ep->bmAttributes = USB_ENDPOINT_XFER_BULK;
	ep->wMaxPacketSize = htole16(size);
}
static void inf_desc(struct usb_interface_descriptor *inf, unsigned int line) {
	inf->bLength = sizeof(struct usb_interface_descriptor);
	inf->bDescriptorType = USB_DT_INTERFACE;
	inf->bNumEndpoints = (unsigned char) (2 * line);
	inf->bInterfaceClass = USB_CLASS_VENDOR_SPEC;
	inf->iInterface = 1;
}
static int open_epfile(int *list, int npoint, const char *str, int slen) {
	char desc[slen + 15];
	for (int i = 1; i < npoint * 2 + 1; i++) {
		sprintf(desc, "%.*s/ep%d", slen, str, i);
		int ep = open(desc, O_RDWR);
		if (ep < 0)
			return -1;
		list[i] = ep;
	}
	return 0;
}
int usbdc_config(const char *str, int slen, int **list, int nline) {
	unsigned int dline = (unsigned int) nline;
	*list = calloc(dline * 2 + 1, sizeof(int)); // ep0 + 2*line
	*list[0] = usbdc_ep_description(nline, str, slen);
	if (*list[0] == -1)
		return -1;
	int st = open_epfile(*list, nline, str, slen);
	if (st != 0) {
		usbdc_log(1, __FILE__, "faile number of end point");
		close(*list[0]);
		free(*list);
		return -1;
	}
	return nline * 2 + 1;
}

static int usbdc_ep_description(const int line, const char *str, int slen) {
	unsigned int dline = (unsigned int) line;
	unsigned int desc_len = sizeof(struct usbdc_pre_deader)
			+ 2 * sizeof(struct usb_interface_descriptor)
			+ 4 * dline * sizeof(struct usb_endpoint_descriptor_no_audio);

	char *udesc = calloc(desc_len, sizeof(char));
	printf("line :%d config len :%d\n", dline, desc_len);
	if (udesc == NULL) {
		puts("cant not maloc data");
	}
	char *pt = udesc;
	struct usbdc_pre_deader *ps = (struct usbdc_pre_deader*) udesc;
	ps->fs_count = htole32(2 * dline + 1);
	ps->hs_count = htole32(2 * dline + 1);
	ps->header.flags = htole32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC);
	ps->header.magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
	ps->header.length = htole32(desc_len);
	pt = pt + sizeof(struct usbdc_pre_deader);
// fs desc
	struct usb_interface_descriptor *fs_inf =
			(struct usb_interface_descriptor*) pt;
	inf_desc(fs_inf, dline);
	pt = pt + sizeof(struct usb_interface_descriptor);
	struct usb_endpoint_descriptor_no_audio *endp;
	for (unsigned int i = 0; i < dline; i++) {
		endp = (struct usb_endpoint_descriptor_no_audio*) pt;
		pt = pt + sizeof(struct usb_endpoint_descriptor_no_audio);
		ep_dec(endp, (unsigned char) (2 * i + 1), USB_DIR_IN, 64);
		endp = (struct usb_endpoint_descriptor_no_audio*) pt;
		pt = pt + sizeof(struct usb_endpoint_descriptor_no_audio);
		ep_dec(endp, (unsigned char) (2 * i + 2), USB_DIR_OUT, 64);
	}
//hs desc
	struct usb_interface_descriptor *hs_inf =
			(struct usb_interface_descriptor*) pt;
	inf_desc(hs_inf, dline);
	pt = pt + sizeof(struct usb_interface_descriptor);
	for (unsigned int i = 0; i < dline; i++) {
		endp = (struct usb_endpoint_descriptor_no_audio*) pt;
		pt = pt + sizeof(struct usb_endpoint_descriptor_no_audio);
		ep_dec(endp, (unsigned char) (2 * i + 1), USB_DIR_IN, 512);
		endp = (struct usb_endpoint_descriptor_no_audio*) pt;
		pt = pt + sizeof(struct usb_endpoint_descriptor_no_audio);
		ep_dec(endp, (unsigned char) (2 * i + 2), USB_DIR_OUT, 512);
	}
	int sl = pt - udesc - (int) desc_len;
	printf("write_len :%d\n", sl);
	char desc[slen + 5];
	sprintf(desc, "%.*s/ep0", slen, str);
	desc[slen + 5] = '\0';
	int ep = open(desc, O_RDWR);
	if (ep < 0) {
		fprintf(stderr, "config_endpoint: cannot open end point\n");
		free(udesc);
		return -1;
	}
	int ret = write(ep, udesc, desc_len);
	free(udesc);
	if (ret < 0) {
		puts("unable do write descriptors");
		goto out;
	}
	if (write(ep, &strings, sizeof(strings)) < 0) {
		puts("unable to write strings");
		goto out;
	}
	return ep;
	out: close(ep);
	return -1;
}
