/********************************************************************************
*
*  File Name     : libusb-test.c
*  Author        : baoli
*  Create Date   : 2017.11.21
*  Version       : 1.0
*  Description   : libusb test
*                  compile command: gcc -o usbtest libusb-test.c -lusb-1.0 -lpthread -lm             
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
#include <libusb-1.0/libusb.h> 



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


int no_device_flag = 0;

unsigned char rev_buf[SEND_BUFF_LEN];
unsigned char send_buf[SEND_BUFF_LEN]; 

libusb_device_handle *dev_handle; //a device handle  



static int bulk_send(char *buf, int num)
{

    int size;
    int rec;    
    
    rec = libusb_bulk_transfer(dev_handle, BULK_SEND_EP, buf, num, &size, 0);  
    if(rec == 0) 
        printf("bulk send sucess,length is:%d\n", size);
    else
        printf("bulk send faild, err: %s\n", libusb_error_name(rec));

    return 0;   
    
}


static int interrupt_send(char *buf, int num)
{

    int size;
    int rec;    
    
    rec = libusb_interrupt_transfer(dev_handle, INT_SEND_EP, buf, num, &size, 0);  
    if(rec == 0) 
        printf("interrupt send sucess,length is:%d\n", size);
    else
        printf("interrupt send faild, err: %s\n", libusb_error_name(rec));

    return 0;   
    
}


void *bulk_rev_thread(void *arg)
{
    int i=0;
    int size;
    int rec;  

    printf("bulk_rev_thread started.\n");  

    while(1)
    {
        if(no_device_flag){
            usleep(50 * 1000);
            continue;
        }

        rec = libusb_bulk_transfer(dev_handle, BULK_RECV_EP, rev_buf, RECV_BUFF_LEN, &size, 0);  
        if(rec == 0) 
        {
            printf("bulk ep rev sucess,length:%d ,data is: %s\n", size, rev_buf);
            printf("hexadecimal is: ");
            for(i=0; i<size; i++)
                printf("0x%x ",rev_buf[i]);
            printf("\n");
        }
        else
        {
            printf("bulk ep rev faild, err: %s\n", libusb_error_name(rec));
            if(rec == LIBUSB_ERROR_IO)
                no_device_flag = 1; //防止一直输出err
        }
    }

    
}


void *interrupt_rev_thread(void *arg)
{
    int i=0;
    int size;
    int rec;  

    printf("interrupt_rev_thread started.\n");  

    while(1)
    {
        if(no_device_flag){
            usleep(50 * 1000);
            continue;
        }

        rec = libusb_interrupt_transfer(dev_handle, INT_RECV_EP, rev_buf, RECV_BUFF_LEN, &size, 0);  
        if(rec == 0) 
        {
            printf("interrupt ep rev sucess,length:%d ,data is: %s\n", size, rev_buf);
            printf("hexadecimal is: ");
            for(i=0; i<size; i++)
                printf("0x%x ",rev_buf[i]);
            printf("\n");
        }
        else
        {
            printf("interrupt ep rev faild, err: %s\n", libusb_error_name(rec));
            if(rec == LIBUSB_ERROR_IO)
                no_device_flag = 1; //防止一直输出err
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

            no_device_flag = 0;          
            printf("usb device open sucess\n\n");
            printf("input data to send:");
            fflush(stdout);
        }
    }
    else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        no_device_flag = 1;
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
    printf("usage: libusb-test [-h] [-b] [-i] [-v vid] [-p pid] \n");
    printf("   -h      : display usage\n");
    printf("   -b      : test bulk transfer\n");   
    printf("   -i      : test interrupt transfer\n"); 
    printf("   -v      : usb VID\n");
    printf("   -p      : usb PID\n"); 

    return;  
 
}
 

//字符串转十六进制
// 0x1234 -> 0x1234   or 1234 -> 0x1234
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
        tmp = hex_str[i] - (((hex_str[i] >= '0') && (hex_str[i] <= '9')) ? '0' : ((hex_str[i] >= 'A') && (hex_str[i] <= 'Z')) ? 'A' - 10 : 'a' - 10);
        sum += tmp * pow(16, 3-i);
    }

    return sum;
}


int main(int argc, char **argv) 
{  
    libusb_context *ctx = NULL; //a libusb session  
    int r; //for return values  
    ssize_t cnt; //holding number of devices in list  
    int opt;
    int test_mode;
    int vid, pid;
    
    pthread_t bulk_rev_thread_id;
    pthread_t int_rev_thread_id;
    pthread_t usb_monitor_thread_id;
    
    test_mode = BULK_TEST;
    vid = VID;
    pid = PID;
   

    while((opt = getopt(argc, argv, "bihv:p:")) != -1)
    {
        switch(opt)
        {
            case 'b' :
            case 'B' :
                test_mode = BULK_TEST;
                break;
            case 'i' :
            case 'I' :
                test_mode = INT_TEST;
                break;
            case 'v' :
            case 'V' :
                vid = str2hex(optarg);
                break;
            case 'p' :
            case 'P' :
                pid = str2hex(optarg);
                break;
            case 'h' :
            case 'H' :
                help();
                return 0;
                break;
            default  :
                printf("unkonw option.\n");
                help();
                return 0;
        }
    }

    printf("\nlibusb test---by baoli\n");
    if(test_mode == BULK_TEST)
        printf("test bulk transfer\n");
    else if(test_mode == INT_TEST)
        printf("test interrupt transfer\n");
    else{
        printf("unkonw test mode.\n");
        return 0;
    }
    printf("usb device: VID:%#06x PID:%#06x\n\n", vid, pid);    //#:输出0x，06:vid或pid第一个数字为0时，输出0x0471而不是0x471


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

    usleep(1000 * 5);   //等线程开始执行后，再执行while

    while(1)
    {
        printf("\ninput data to send:");

        fgets(send_buf, SEND_BUFF_LEN, stdin);

        if(test_mode == BULK_TEST)
            bulk_send(send_buf, strlen(send_buf)-1);
        else if(test_mode == INT_TEST)
            interrupt_send(send_buf, strlen(send_buf)-1);
        else
            ;
    }  
     
}  
