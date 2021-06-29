To use mylib on embed device ; it required libaio and "FunctionFileSystem" .
with buildroot you can see in https://elinux.org/images/e/ef/USB_Gadget_Configfs_API_0.pdf  
and ensure that you choise same that
<pre>
&lt;M&gt;   USB Gadget precomposed configurations                                   
                < >     Gadget Zero (DEVELOPMENT)                                             
                < >     Audio Gadget                                                          
                < >     Ethernet Gadget (with CDC Ethernet support)                           
                < >     Network Control Model (NCM) support                                   
                < >     Gadget Filesystem                                                     
                &lt;M&gt;     Function Filesystem   
</pre>
to the module : config this with
<pre>
	 modprobe g_ffs idVendor=0x0129 idProduct=0x00fa \
                iSerialNumber=0x1591997 \
                iManufacturer="Amneiht" \
                iProduct="Ffs test" \
                functions=pika
      mkdir /dev/ffs
      mount -t functionfs pika /dev/ffs
</pre>
While you use "USB Gadget precomposed configurations" you can mount functionfs only one time . But i thing it not be a problem , becase we can modify interfaces , endpoint ,... that we want

