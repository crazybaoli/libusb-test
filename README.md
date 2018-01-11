## libusb-test

### 1.compile command
```
chmod +x build.sh
./build.sh 
or
gcc -o libusb libusb-test.c -lusb-1.0 -lpthread -lm
```

### 2.功能
- 支持bulk/interrupt endpoint 数据读写
- 支持hotplug
- 支持命令行参数
- 支持快捷发送数据
- 支持将收到的数据保存为文件
- 支持'lsusb'功能，可列出系统所有usb设备
- 支持打印显示特定usb设备（VID:PID）的描述符

### 3.使用实例
#### 1. help
	./libusb -h
#### 2. list usb devices
	sudo ./libusb -l
>bus: 001 device: 001, VID: 1d6b PID: 0002, EHCI Host Controller
bus: 002 device: 064, VID: 0471 PID: 0999
bus: 002 device: 003, VID: 0e0f PID: 0002
bus: 002 device: 002, VID: 0e0f PID: 0003, VMware Virtual USB Mouse
bus: 002 device: 001, VID: 1d6b PID: 0001, UHCI Host Controller
#### 3. print descriptor
	sudo ./libusb -v 0471 -p 0999 -a
this wil print: device descriptor，configuration descriptor, interface descriptor, endpoint descriptor.
#### 4. bulk transfer
	sudo ./libusb -b
	or
	sudo ./libusb
#### 5. interrupt transfer
	sudo ./libusb -i
#### 6. save as file
	sudo ./libusb -f
	or 
	sudo ./libusb -ffilepath
#### 7. send data quickly
enter 'test64' when the program is running. 
also enter test128, test200, etc. 
![@send 63 bytes data quickly ](http://img.hb.aicdn.com/ab533da26e9a3d279fa59b8f2b21001ca5ea276e863b-QtoXo7_fw658)

