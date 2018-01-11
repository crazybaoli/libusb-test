/********************************************************************************
*
*  File Name     : libusb-test.c
*  Author        : baoli
*  Create Date   : 2017.11.21
*  Version       : 1.0
*  Description   : libusb test
*                  compile command: gcc -o libusb libusb-test.c -lusb-1.0 -lpthread -lm             
*  History       : 1. Data:
*                     Author:
*                     Modification:
*
********************************************************************************/


#include <unistd.h>  
#include <stdio.h>  
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <semaphore.h>

#include <libusb-1.0/libusb.h> 


typedef unsigned char uchar;
typedef unsigned int  uint;


#define VID 0x0471
#define PID 0x0999

#define BULK_RECV_EP    0x82
#define BULK_SEND_EP    0x02
#define INT_RECV_EP     0x81
#define INT_SEND_EP     0x01

#define SEND_BUFF_LEN    500
#define RECV_BUFF_LEN    500

#define BULK_TEST   1
#define INT_TEST    2

#define SAVE_FILE_PATH  "libusb_test_recv_data"


sem_t print_sem; //信号量
FILE *g_file_stream = NULL;
int g_no_device_flag = 0;
int g_file_save_en = 0; //文件保存使能标志
int g_send_flag = 0;    //调用数据发送函数标志

uchar rev_buf[SEND_BUFF_LEN];   //usb 接收缓冲区
uchar send_buf[SEND_BUFF_LEN];  //usb发送缓冲区 

libusb_device_handle *dev_handle;

char test_bytes[] = 
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n"
"abcdefghijklmnopqrstuvwxyz0123456789\n";   //37*15=555 bytes



int  bulk_send(char *buf, int num);
int  interrupt_send(char *buf, int num);
void *bulk_rev_thread(void *arg);
void *interrupt_rev_thread(void *arg);
int  LIBUSB_CALL usb_event_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data);
void help(void);
int  str2hex(char *hex);
void hex2str(uchar *pbDest, uchar *pbSrc, ushort nLen);
void *usb_monitor_thread(void *arg);
int  save_to_file(FILE *file_stream, uchar *data, int length);
void print_devs(libusb_device **devs);
int  list_devices(void);
void print_endpoint(const struct libusb_endpoint_descriptor *endpoint);
void print_altsetting(const struct libusb_interface_descriptor *interface);
void print_interface(const struct libusb_interface *interface);
void print_configuration(struct libusb_config_descriptor *config);
void print_device(libusb_device *dev, struct libusb_device_descriptor *desc);
int  print_descriptor(libusb_device *dev);
int  get_descriptor_with_vid_pid(int vid, int pid);
void sigint_handler(int sig);


//采用bulk端点发送数据
int bulk_send(char *buf, int num)
{

    int size;
    int rec;    
    
    g_send_flag = 1;
    rec = libusb_bulk_transfer(dev_handle, BULK_SEND_EP, buf, num, &size, 0);  
    if(rec == 0) {
        printf("bulk send sucess,length: %d bytes\n", size);
        sem_post(&print_sem);
    }
    else{
        g_send_flag = 0;
        printf("bulk send faild, err: %s\n", libusb_error_name(rec));
    }

    return 0;   
    
}


//采用interrupt端点发送数据
int interrupt_send(char *buf, int num)
{

    int size;
    int rec;    
    
    g_send_flag = 1;
    rec = libusb_interrupt_transfer(dev_handle, INT_SEND_EP, buf, num, &size, 0);  
    if(rec == 0) {
        printf("interrupt send sucess, length: %d bytes\n", size);
        sem_post(&print_sem);
    }
    else{
        g_send_flag = 0;
        printf("interrupt send faild, err: %s\n", libusb_error_name(rec));
    }

    return 0;   
    
}


//bulk 端点接收线程
void *bulk_rev_thread(void *arg)
{
    int i=0;
    int size;
    int rec;  
    int save_bytes;
    char hex_buf[RECV_BUFF_LEN*5];

    printf("bulk_rev_thread started.\n");  

    while(1)
    {
        if(g_no_device_flag){
            usleep(50 * 1000);
            continue;
        }

        memset(rev_buf, 0, RECV_BUFF_LEN);
        rec = libusb_bulk_transfer(dev_handle, BULK_RECV_EP, rev_buf, RECV_BUFF_LEN, &size, 0);  
        if(rec == 0) 
        {
            if(g_send_flag == 1)  //考虑如果没有调用发送数据函数，接收到数据后调用sem_wait会一直等待直到调用了bulk_send，才会继续往下执行
            {
                g_send_flag = 0;
                sem_wait(&print_sem);
            }

            printf("\nbulk ep rev sucess, length: %d bytes. \n", size);

            if(g_file_save_en){
                save_bytes = save_to_file(g_file_stream, rev_buf, size);            
                printf("save %d bytes to file.\n", save_bytes);
            }

            hex2str(hex_buf, rev_buf, size);           
            printf("data is: \n%s\n", rev_buf);
            printf("hex is: \n%s\n", hex_buf);
        }
        else
        {
            printf("bulk ep rev faild, err: %s\n", libusb_error_name(rec));
            if(rec == LIBUSB_ERROR_IO)
                g_no_device_flag = 1; //防止一直输出err
        }
    }
    
}


//interrupt 端点接收线程
void *interrupt_rev_thread(void *arg)
{
    int i=0;
    int size;
    int rec;  
    int save_bytes;
    char hex_buf[RECV_BUFF_LEN*5];

    printf("interrupt_rev_thread started.\n");  

    while(1)
    {
        if(g_no_device_flag){
            usleep(50 * 1000);
            continue;
        }

        memset(rev_buf, 0, RECV_BUFF_LEN);
        rec = libusb_interrupt_transfer(dev_handle, INT_RECV_EP, rev_buf, RECV_BUFF_LEN, &size, 0);  
        if(rec == 0) 
        {
            if(g_send_flag == 1)  //考虑如果没有调用发送数据函数，接收到数据后调用sem_wait会一直等待直到调用了interrupt_send，才会继续往下执行
            {
                g_send_flag = 0;
                sem_wait(&print_sem);
            }

            printf("\ninterrupt ep rev sucess, length: %d bytes. \n", size);

            if(g_file_save_en){
                save_bytes = save_to_file(g_file_stream, rev_buf, size);            
                printf("save %d bytes to file.\n", save_bytes);
            }

            hex2str(hex_buf, rev_buf, size);            
            printf("data is: \n%s\n", rev_buf);
            printf("hex is: \n%s\n", hex_buf);
        }
        else
        {
            printf("interrupt ep rev faild, err: %s\n", libusb_error_name(rec));
            if(rec == LIBUSB_ERROR_IO)
                g_no_device_flag = 1; //防止一直输出err
        }
    }

    
}


//usb hotplugin callback function
int LIBUSB_CALL usb_event_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data) 
{
    struct libusb_device_descriptor desc;
    int r;
    
    printf("\nusb hotplugin event.\n");

    r = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS != r) {
        printf("error getting device descriptor.\n");
    }
    
    if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
        printf("usb device attached: %04x:%04x\n", desc.idVendor, desc.idProduct);
        
        if (dev_handle) {
            libusb_close(dev_handle);
            dev_handle = NULL;
        }
        
        r = libusb_open(dev, &dev_handle);
        if (LIBUSB_SUCCESS != r) {
            printf ("error opening device.\n");
        }
        else {
            if(libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
                printf("kernel driver active\n");
                if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
                    printf("kernel driver detach\n");
            }
            int r = libusb_claim_interface(dev_handle, 0); //claim interface 0 (the first) of device (mine had jsut 1)
            if (r < 0) {
                printf("libusb_claim_interface failed\n");
                return 0;
            }

            g_no_device_flag = 0;          
            printf("usb device open sucess\n\n");
            printf("input data to send or command:");
            fflush(stdout);
        }
    }
    else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        g_no_device_flag = 1;
        printf("usb device removed: %04x:%04x\n", desc.idVendor, desc.idProduct);
        if (dev_handle) {           
            libusb_close(dev_handle);
            dev_handle = NULL;

        }
    }
    else
        ;

    return 0;
}


//usb hotplugin monitor thread
void * usb_monitor_thread(void *arg) 
{ 
    printf("usb monitor thread started.\n");

    int r = 0;
    while (1) {
        r = libusb_handle_events(NULL);
        if (r < 0)
            printf("libusb_handle_events() failed: %s\n", libusb_error_name(r));
    }  

}


void help(void)
{
    printf("usage: libusb-test [-h] [-b] [-i] [-v vid] [-p pid] [-ffile_path] [-l] [-a] \n");
    printf("   -h      : display usage\n");
    printf("   -b      : test bulk transfer\n");   
    printf("   -i      : test interrupt transfer\n"); 
    printf("   -v      : usb VID\n");
    printf("   -p      : usb PID\n"); 
    printf("   -f      : file path to save data\n"); 
    printf("   -l      : list usb devices\n");
    printf("   -a      : print descriptor for the usb(VID:PID)\n");

    return;  
 
}
 

//将char类型（0-255）的buf转换成以十六进制显示的字符串。
//使用这个函数可以避免多次循环调用printf("%x", buf[i]); 提高效率，并且能避免线程切换造成输出乱序的问题。
//buf={'1', '2', '3', 'a', 'b', 'c'} -> "0x31 0x32 0x33 0x61 0x62 0x63"
//buf={0,1,2,3} -> "0x00 0x01 0x02 0x03"
void hex2str(uchar *dest, uchar *src, ushort nLen)
{
    int i;

    for (i=0; i<nLen; i++)
    {
        sprintf(dest+5*i, "%#04x ", src[i]);    //string类函数，不会造成线程切换
    }

    dest[nLen*5] = '\0';
}


//十六进制数字字符串转数字（十六进制）
// "0x1234" -> 0x1234   or "1234" -> 0x1234
int str2hex(char *hex) 
{
    int sum = 0;
    int tmp = 0;
    char hex_str[5];

    if(strlen(hex) == 6)    //0x1234
        memcpy(hex_str, &hex[2], 4);
    else
        memcpy(hex_str, hex, 4);

    for(int i = 0; i < 4; i++)
    {
        tmp = hex_str[i] - (((hex_str[i] >= '0') && (hex_str[i] <= '9')) ? '0' : \
            ((hex_str[i] >= 'A') && (hex_str[i] <= 'Z')) ? 'A' - 10 : 'a' - 10);
        sum += tmp * pow(16, 3-i);
    }

    return sum;
}



//将接收到数据保存到文件中
int save_to_file(FILE *file_stream, uchar *data, int length)
{
    int write_num;
    write_num = fwrite(data, 1, length, file_stream);
    fputc('\n', file_stream);   //为每段数据换行，方便查看
    fflush(file_stream);

    return write_num;

}



void print_devs(libusb_device **devs)
{
    libusb_device *dev;
    int i = 0, j = 0;
    uint8_t path[8]; 
    libusb_device_handle *handle = NULL;
    char string[256];

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            printf("failed to get device descriptor\n");
            return;
        }

        printf("bus: %03d device: %03d, VID: %04x PID: %04x", 
            libusb_get_bus_number(dev), libusb_get_device_address(dev), desc.idVendor, desc.idProduct);

        int ret = libusb_open(dev, &handle);
        if (LIBUSB_SUCCESS == ret) {
            if (desc.iProduct) {
                ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));    //显示产品信息
                if (ret > 0)   
                    printf(", %s", string);
            }

        }
        printf("\n");

        if (handle)
            libusb_close(handle);

    }
}

//列出系统所有的USB设备.
//包括：bus number. device number, vid, pid, product info
int list_devices(void)
{
    libusb_device **devs;
    int r;
    ssize_t cnt;

    r = libusb_init(NULL);
    if (r < 0)
        return r;

    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
        return (int) cnt;

    print_devs(devs);
    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);
    return 0;   
}



void print_endpoint(const struct libusb_endpoint_descriptor *endpoint)
{
    int i, ret;

    printf("      Endpoint descriptor:\n");
    printf("        bEndpointAddress: %02xh\n", endpoint->bEndpointAddress);
    printf("        bmAttributes:     %02xh\n", endpoint->bmAttributes);
    printf("        wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
    printf("        bInterval:        %d\n", endpoint->bInterval);
    printf("        bRefresh:         %d\n", endpoint->bRefresh);
    printf("        bSynchAddress:    %d\n", endpoint->bSynchAddress);

}

void print_altsetting(const struct libusb_interface_descriptor *interface)
{
    int i;

    printf("    Interface descriptor:\n");
    printf("      bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
    printf("      bAlternateSetting:  %d\n", interface->bAlternateSetting);
    printf("      bNumEndpoints:      %d\n", interface->bNumEndpoints);
    printf("      bInterfaceClass:    %d\n", interface->bInterfaceClass);
    printf("      bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
    printf("      bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
    printf("      iInterface:         %d\n", interface->iInterface);

    for (i = 0; i < interface->bNumEndpoints; i++)
        print_endpoint(&interface->endpoint[i]);
}


void print_interface(const struct libusb_interface *interface)
{
    int i;

    for (i = 0; i < interface->num_altsetting; i++)
        print_altsetting(&interface->altsetting[i]);
}

void print_configuration(struct libusb_config_descriptor *config)
{
    int i;

    printf("  Configuration descriptor:\n");
    printf("    wTotalLength:         %d\n", config->wTotalLength);
    printf("    bNumInterfaces:       %d\n", config->bNumInterfaces);
    printf("    bConfigurationValue:  %d\n", config->bConfigurationValue);
    printf("    iConfiguration:       %d\n", config->iConfiguration);
    printf("    bmAttributes:         %02xh\n", config->bmAttributes);
    printf("    MaxPower:             %d\n", config->MaxPower);

    for (i = 0; i < config->bNumInterfaces; i++)
        print_interface(&config->interface[i]);
}


void print_device(libusb_device *dev, struct libusb_device_descriptor *desc)
{
    int i;

    printf("Device descriptor:\n");
    printf("  bDescriptorType:         %d\n", desc->bDescriptorType);
    printf("  bcdUSB:                  %#06x\n", desc->bcdUSB);
    printf("  bDeviceClass:            %d\n", desc->bDeviceClass);
    printf("  bDeviceSubClass:         %d\n", desc->bDeviceSubClass);
    printf("  bDeviceProtocol:         %d\n", desc->bDeviceProtocol);
    printf("  bMaxPacketSize0:         %d\n", desc->bMaxPacketSize0);
    printf("  idVendor:                %#06x\n", desc->idVendor);
    printf("  idProduct:               %#06x\n", desc->idProduct);
    printf("  bNumConfigurations:      %d\n", desc->bNumConfigurations);


    for (i = 0; i < desc->bNumConfigurations; i++) {
        struct libusb_config_descriptor *config;
        int ret = libusb_get_config_descriptor(dev, i, &config);
        if (LIBUSB_SUCCESS != ret) {
            printf("Couldn't retrieve descriptors\n");
            continue;
        }

        print_configuration(config);
        libusb_free_config_descriptor(config);
    }
}


//打印USB设备的描述符
int print_descriptor(libusb_device *dev)
{
    struct libusb_device_descriptor desc;
    int ret, i;

    ret = libusb_get_device_descriptor(dev, &desc);
    if (ret < 0) {
        printf("failed to get device descriptor\n");
        return -1;
    }

    print_device(dev, &desc);       

}


//根据VID:PID，打印特定USB设备的描述符
int get_descriptor_with_vid_pid(int vid, int pid)
{
    libusb_device **devs;
    libusb_device *found = NULL;
    libusb_device *dev;
    ssize_t cnt;
    int r, i;

    r = libusb_init(NULL);
    if (r < 0)
        return -1;

    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
        return -1;

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0)
            return -1;
        if (desc.idVendor == vid && desc.idProduct == pid) {
            found = dev;
            break;
        }
    }

    print_descriptor(found);

    libusb_free_device_list(devs, 1);

    libusb_exit(NULL);
    return 0;    
}


//ctrl+c 信号处理函数
void sigint_handler(int sig)
{
    if(g_file_stream)
        fclose(g_file_stream);

    printf("\nlibusb test quit.\n");
    exit(0);
}



int main(int argc, char **argv) 
{  
    int r;  
    int opt;
    int test_mode;
    int vid, pid;
    int send_length;

    libusb_context *ctx = NULL;

    struct sigaction act;
   
    pthread_t bulk_rev_thread_id;
    pthread_t int_rev_thread_id;
    pthread_t usb_monitor_thread_id;
    
    test_mode = BULK_TEST;
    vid = VID;
    pid = PID;
    g_file_save_en = 0;
    g_send_flag = 0;

    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
   
    //命令行参数解析
    while((opt = getopt(argc, argv, "bif::hv:p:la")) != -1)
    {
        switch(opt)
        {
            case 'b' :
                test_mode = BULK_TEST;
                break;
            case 'i' :
                test_mode = INT_TEST;
                break;
            case 'f' :
                g_file_stream = fopen(optarg == NULL ? SAVE_FILE_PATH : optarg, "wb");    //写打开，并将文件长度截为0，没有则新建
                if(g_file_stream)  
                    g_file_save_en = 1;
                else
                    perror("file open faild: ");    
                break;                
            case 'v' :
                vid = str2hex(optarg);
                break;
            case 'p' :
                pid = str2hex(optarg);
                break;
            case 'l' :
                list_devices();
                exit(0);
                break; 
            case 'a' :
                get_descriptor_with_vid_pid(vid, pid);
                exit(0);
                break;                                               
            case 'h' :
                help();
                return 0;
                break;
            default  :
                printf("unkonw option.\n");
                help();
                return 0;
        }
    }


    printf("\nlibusb test\n");
    if(test_mode == BULK_TEST)
        printf("test bulk transfer\n");
    else if(test_mode == INT_TEST)
        printf("test interrupt transfer\n");
    else{
        printf("unkonw test mode.\n");
        return 0;
    }
    printf("usb device: VID:%#06x PID:%#06x\n\n", vid, pid);    //#:输出0x，06:vid或pid第一个数字为0时，输出0x0471而不是0x471

    r = sem_init(&print_sem, 0, 0); //初始化信号量，用于解决多线程下printf输出乱序的问题
    if(r != 0)
        perror("sem_init faild: ");

    r = libusb_init(&ctx); //initialize the library for the session we just declared  
    if(r < 0) {  
        printf("libusb init faild, err:%s\n", libusb_error_name(r)); //there was an error  
        return -1;  
    } 

    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_WARNING); 

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        printf("hotplug capabilites are not supported on this platform.\n");
        libusb_exit (NULL);
        return -1;
    }

    r = libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, LIBUSB_HOTPLUG_NO_FLAGS, vid, pid, LIBUSB_HOTPLUG_MATCH_ANY, usb_event_callback, NULL, NULL);
    if (LIBUSB_SUCCESS != r) {
        printf("error registering callback: %s\n", libusb_error_name(r));
        libusb_exit(ctx);
        return -1;
    }   
    
    dev_handle = libusb_open_device_with_vid_pid(ctx, vid, pid);  //open a usb device with VID&PID
    if(dev_handle == NULL)  {
        printf("cannot open device\n");  
        return -1;
    }
    else  
        printf("usb device opened.\n");  
  
    r = libusb_kernel_driver_active(dev_handle, 0);
    if(r == 0)  //ok
        ;
    else if(r == 1) { 
        printf("Kernel driver is active, now try detached\n");  
        if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it  
            printf("Kernel driver is detached!\n");
        else{
            printf("libusb_detach_kernel_driver, err:%s\n", libusb_error_name(r));
            return -1;
        }  
    }
    else{
        printf("libusb_kernel_driver_active, err:%s\n", libusb_error_name(r));
        return -1;
    } 

    r = libusb_claim_interface(dev_handle, 0); //claim interface 0 ,stm32采用接口0
    if(r < 0) {  
        printf("cannot claim interface, err:%s\n", libusb_error_name(r));
        return -1;  
    }  

    //热插拔监听
   r = pthread_create(&usb_monitor_thread_id, 0, usb_monitor_thread, 0); 
    if(r != 0 )
    {
        perror("usb_monitor_thread creation faild\n");
    } 

    //new thread,usb rev data
    if(test_mode == BULK_TEST){
        r = pthread_create(&bulk_rev_thread_id, NULL, bulk_rev_thread, NULL);
        if(r != 0 )
        {
            perror("thread creation faild\n");
        }
    }
    else if(test_mode == INT_TEST){
        r = pthread_create(&int_rev_thread_id, NULL, interrupt_rev_thread, NULL);
        if(r != 0 )
        {
            perror("thread creation faild\n");
        }       
    }


    while(1)
    {
        usleep(10 * 1000);
        printf("\ninput data to send or command:");

        memset(send_buf, 0, SEND_BUFF_LEN);
        fgets(send_buf, SEND_BUFF_LEN, stdin);
        send_length = strlen(send_buf)-1;
        send_buf[send_length] = '\0' ;   //将读入的换行符转为\0

        //用于快捷发送数据，如输入test64，会自动发送64字节数据，输入test100则会自动发送100字节数据
        if(strncmp(send_buf, "test", 4) == 0){
            send_length = atoi(&send_buf[4]);
            if(send_length > strlen(test_bytes)){
                printf("send_length:%d should less than max_length:%ld\n", send_length, strlen(test_bytes));
                continue;
            }
            printf("test sending %d bytes.\n", send_length);
            memcpy(send_buf, test_bytes, send_length);
        }

        if(test_mode == BULK_TEST)
            bulk_send(send_buf, send_length);
        else if(test_mode == INT_TEST)
            interrupt_send(send_buf, send_length);
        else
            ;
    }  
     
}  
