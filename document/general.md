in my project : 
usb_device is source code write for embed system 
usb_host is source code write for host machine with support libusb
<br>
it has two important you must remember : usbdc_handle and usbdc_line

usbdc_handle is a struct contain some infomation about endpoint , device , etc ,..
and usbdc_line contain data about two enpoint in and out ,
i has writen some funtion for it make the write and read action is asynchous and simple 

<br>
with usbdc_handle you should call usbdc_handle_checkevt before do anything . be cause idon't want create new thread to handle . but it is no problem if you create a loop  same that
usbdc_handle * han ;
#---do something-----
while(run)
{
	usbdc_handle_checkevt(han);
	usleep(100000);
}

<br>
with usbdc_line , you can read or wirte data  on it but ,you should call usbdc_line_is_write_ready() or usbdc_line_is_read_ready()
to ensure that you can write/read on it

the max data can write/read is USB_MAX , the lib will return error if it more than USB_MAX and less than 0

example :
if (usbdc_line_is_read_ready(ls)) {
	int ret = usbdc_line_read(ls, buff, 1000);
	buff[ret] = '\0';
	puts(buff);
} else if (usbdc_line_is_read_timeout(ls, 10000)) {
	usbdc_line_read_cancel(ls);
}
//todo : more example