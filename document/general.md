</p>in my project : 
usb_device is source code write for embed system 
usb_host is source code write for host machine with support libusb
<br>
it has two important you must remember : usbdc_handle and usbdc_line

usbdc_handle is a struct contain some infomation about endpoint , device , etc ,..
and usbdc_line contain data about two enpoint in and out ,
i has writen some funtion for it make the write and read action is asynchous and simple 

<br>
with usbdc_handle you should call usbdc_handle_checkevt before do anything . because some device cannot create mutithread . it you device is strong you can create thead with loop sane that or use high level api is usbdc_stream
<pre>
usbdc_handle * han ;
#---do something-----
while(run)
{
	usbdc_handle_checkevt(han);
	usleep(100000);
}
</pre>
<br>
with usbdc_line , you can read or wirte data  on it but ,you should call usbdc_line_is_write_ready() or usbdc_line_is_read_ready()
to ensure that you can write/read on it

the max data can write/read is USB_MAX , the lib will return error if it more than USB_MAX and less than 0

example :
<pre>
if (usbdc_line_is_read_ready(ls)) {
	int ret = usbdc_line_read(ls, buff, 1000);
	buff[ret] = '\0';
	puts(buff);
} else if (usbdc_line_is_read_timeout(ls, 10000)) {
	usbdc_line_read_cancel(ls);
}
</pre>
becareful
on now my lib is not savefull for mutithreading , It mean that if more than one thread write or read on the 1 line/stream in same time , it will be crash or cannot kill
because it can create 32 enpoint aka 16 line . I thing . it will be enough for mutihreading

//todo : more example
