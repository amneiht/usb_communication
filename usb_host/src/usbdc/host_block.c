/*
 * usbdc_block.c
 *
 *  Created on: Jun 5, 2021
 *      Author: amneiht
 */
//
//#include <usb_dc.h>
//
//int usbdc_line_block_write(usbdc_line *line, char *buff, int slen, int timeout) {
//	return 0;
//}
//int usbdc_line_block_read(usbdc_line *line, char *buff, int slen, int timeout) {
//	if (usbdc_line_is_read_ready(line)) {
//		return usbdc_line_read(line, buff, slen);
//	} else {
//		while (1) {
//			usbdc_handle_checkevt(line->ctx);
//			return 0;
//		}
//	}
//}
