#ifndef USB_H
#define USB_H

// USB-related function declarations and definitions go here

#define USB_Factory_STM 1 
#define USB_Factory_NXP 0 

typedef struct{

    int device_id;                    // Unique identifier for the USB device
    int vendor_id;                    // Vendor ID of the USB device            
    int product_id;                   // Product ID of the USB device

    //这个结构体可以根据实际需要添加更多的USB设备功能
    void (*usb_init)(void);
    void (*usb_connect)(void);
    void (*usb_disconnect)(void);
    void (*usb_transfer_data)(const void *data, size_t size);
    void (*usb_receive_data)(void *buffer, size_t size);
    void (*usb_set_configuration)(int config);
    void (*usb_get_device_descriptor)(void *descriptor, size_t size);
    void (*usb_set_address)(int address);
    void (*usb_control_transfer)(int request_type, int request, int value, int index, void *data, size_t size);
    void (*usb_handle_interrupt)(void);
    void (*usb_reset)(void);
}HAL_USB_Struct;


void stm_usb_init(void);//这里是举例，这可以改为其他头文件声明的函数名

//下面的函数为HAL层对USB功能的封装
void usb_init(void);
void usb_connect(void);
void usb_disconnect(void);
void usb_transfer_data(const void *data, size_t size);
void usb_receive_data(void *buffer, size_t size);
void usb_set_configuration(int config);
void usb_get_device_descriptor(void *descriptor, size_t size);
void usb_set_address(int address);
void usb_control_transfer(int request_type, int request, int value, int index, void *data, size_t size);
void usb_handle_interrupt(void);
void usb_reset(void);


#endif // USB_H