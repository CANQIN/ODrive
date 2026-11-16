#include "usbd_core.h"
#include "cubemx_hooks.h"

USBD_StatusTypeDef odrive_cubemx_usb_ll_init(USBD_HandleTypeDef* device);

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* device) {
    USBD_StatusTypeDef status = odrive_cubemx_usb_ll_init(device);
    if (status == USBD_OK) {
        odrive_usb_configure_fifos((PCD_HandleTypeDef*)device->pData);
    }
    return status;
}
