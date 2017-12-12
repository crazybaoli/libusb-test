/********************************************************************************
*
*  File Name     : libusb-test.c
*  Author        : baoli
*  Create Date   : 2017.11.21
*  Version       : 1.0
*  Description   : libusb test
*                  compile command: gcc -o usbtest usbtest.c -lusb-1.0 -lpthread             
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
#include <libusb-1.0/libusb.h> 


#define VID 0x0471
#define PID 0x0999

#define BULK_RECV_EP    0x82
#define BULK_SEND_EP    0x02
#define INT_RECV_EP     0x81
#define INT_SEND_EP     0x01

#define SEND_BUFF_LEN    100
#define RECV_BUFF_LEN    100

int no_device_flag = 0;

unsigned char rev_buf[SEND_BUFF_LEN];
unsigned char send_buf[SEND_BUFF_LEN]; 

libusb_device_handle *dev_handle; //a device handle  


static int bulk_send(libusb_device_handle *hd, char *buf, int num, int timeout)
{

    int size;
    int rec;    
    
    rec = libusb_bulk_transfer(hd, BULK_SEND_EP, buf, num, &size, timeout);  
    if(rec == 0) 
        printf("bulk send sucess,length is:%d\n", size);
    else
        printf("bulk send faild, err: %s\n", libusb_error_name(rec));

    return 0;   
    
}


void *bulk_rev(void *arg)
{
    int i=0;
    int size;
    int rec;    

    
    while(1)
    {
        if(no_device_flag){
            usleep(50 * 1000);
            continue;
        }

        rec = libusb_bulk_transfer(dev_handle, BULK_RECV_EP, rev_buf, RECV_BUFF_LEN, &size, 0);  
        if(rec == 0) 
        {
             printf("rev sucess,length:%d ,data is: %s\n",size,rev_buf);
             printf("hexadecimal is: ");
             for(i=0; i<size; i++)
               printf("0x%x ",rev_buf[i]);
            printf("\n\n");
        }
        else
        {
            printf("rev faild, err: %s\n", libusb_error_name(rec));
            if(rec == LIBUSB_ERROR_NO_DEVICE)
                pthread_exit(NULL);
        }
    }

    
}


int LIBUSB_CALL usb_event_callback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data) {
    struct libusb_device_descriptor desc;
    int rc;
    
    printf("usb hotplugin event.\n");

    rc = libusb_get_device_descriptor(dev, &desc);
    if (LIBUSB_SUCCESS != rc) {
        printf("Error getting device descriptor\n");
    }
    
    if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
        printf("usb device attached: %04x:%04x\n", desc.idVendor, desc.idProduct);
        
        if (dev_handle) {
            libusb_close(dev_handle);
            dev_handle = NULL;
        }
        
        rc = libusb_open(dev, &dev_handle);
        if (LIBUSB_SUCCESS != rc) {
            printf ("Error opening device\n");
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
            printf("usb device open succ\n");
        }
    }
    else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        no_device_flag = 1;
        printf("usb device removed: %04x:%04x\n", desc.idVendor, desc.idProduct);
        if (dev_handle) {

            int r = libusb_release_interface(dev_handle, 0);
            if(r != 0) {
                printf("cannot release interface\n");
            }
            
            libusb_attach_kernel_driver(dev_handle, 0);
            libusb_close(dev_handle);
            dev_handle = NULL;

        }
    }
    else
        ;

    return 0;
}


//usb hotplugin monitor thread
void * usb_monitor_thread(void *arg) { 
    printf("usb monitor thread started.\n");

    int r = 0;
    while (1) {
        r = libusb_handle_events(NULL);
        if (r < 0)
            printf("libusb_handle_events() failed: %s\n", libusb_error_name(r));
    }  

    //libusb_exit(ctx);
    printf("usb monitor thread exited.\n");
}


  
int main() {  
    libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices  
    libusb_context *ctx = NULL; //a libusb session  
    int r; //for return values  
    ssize_t cnt; //holding number of devices in list  
    
    pthread_t a_thread;
    pthread_t usb_monitor_thread_id;
    
    printf("\nlibusb test v3.0---by baoli\n\n");

    r = libusb_init(&ctx); //initialize the library for the session we just declared  
    if(r < 0) {  
        printf("libusb init faild, err:%s\n", libusb_error_name(r)); //there was an error  
        return 1;  
    } 

    libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_WARNING); 

    r = libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, LIBUSB_HOTPLUG_NO_FLAGS, VID, PID, LIBUSB_HOTPLUG_MATCH_ANY, usb_event_callback, NULL, NULL);
    if (LIBUSB_SUCCESS != r) {
        printf("error registering callback: %s\n", libusb_error_name(r));
        libusb_exit(ctx);
        return 1;
    }   
    
    dev_handle = libusb_open_device_with_vid_pid(ctx, VID, PID);  //open a usb device with VID&PID
    if(dev_handle == NULL)  {
        printf("Cannot open device\n");  
        return 1;
    }
    else  
        printf("Device Opened\n");  
  
    r = libusb_kernel_driver_active(dev_handle, 0);
    if(r == 0)  //ok
        ;
    else if(r == 1) { 
        printf("Kernel driver is active, now try detached\n");  
        if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it  
            printf("Kernel driver is detached!\n");
        else{
            printf("libusb_detach_kernel_driver, err:%s\n", libusb_error_name(r));
            return 1;
        }  
    }
    else{
        printf("libusb_kernel_driver_active, err:%s\n", libusb_error_name(r));
        return 1;
    } 

    r = libusb_claim_interface(dev_handle, 0); //claim interface 0 ,stm32采用接口0
    if(r < 0) {  
        printf("cannot claim interface, err:%s\n", libusb_error_name(r));
        return 1;  
    }  

    //热插拔监听
   r = pthread_create(&usb_monitor_thread_id, 0, usb_monitor_thread, 0); 
    if(r != 0 )
    {
        perror("usb_monitor_thread creation faild\n");
    } 

    //new thread,usb rev data
    r = pthread_create(&a_thread, NULL, bulk_rev, NULL);
    if(r != 0 )
    {
        perror("thread creation faild\n");
    }

    while(1)
    {
        printf("input data to send:\n");
        fgets(send_buf, SEND_BUFF_LEN, stdin);
        bulk_send(dev_handle, send_buf, strlen(send_buf)-1, 0);
    }

  
    libusb_close(dev_handle); //close the device
    libusb_exit(ctx); //needs to be called to end the  
  
    return 0;  
}  
