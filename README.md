## libusb-test

### 1.compile command: 
	gcc -o libusb-test libusb-test.c -lusb-1.0 -lpthread -lm

### 2.功能： 
- 支持bulk/interrupt endpoint 数据读写
- 支持hotplug
- 支持命令行参数
- 支持快捷发送数据
- 支持将收到的数据保存为文件