#include "usb.h"


//平台的兼容性，对不同平台的USB平台SDK驱动进行初始化，并用宏开关做使用权限
#if USB_Factory_STM
HAL_USB_Struct hal_usbstruct = {
    
    .usb_init = stm_usb_init,
};
#endif

//对于硬件层，需要按照这种模式实现不同芯片平台的快速兼容





