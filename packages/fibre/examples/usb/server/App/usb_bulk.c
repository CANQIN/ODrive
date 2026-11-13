#include "usb_bulk.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"
#include "main.h"

#define EP_OUT 0x01U
#define EP_IN 0x81U
static USBD_HandleTypeDef usb;
static uint8_t rx_buffer[64], tx_buffer[64];
static volatile uint32_t generation, rx_length;
static volatile uint8_t tx_busy, tx_done;

// 自定义接口，两个 Bulk 端点，不包含 CDC 或 HID 类。
static uint8_t config_desc[] = {
    9, USB_DESC_TYPE_CONFIGURATION, 32, 0, 1, 1, 0, 0xC0, 50,
    9, USB_DESC_TYPE_INTERFACE, 0, 0, 2, 0xFF, 0, 0, 0,
    7, USB_DESC_TYPE_ENDPOINT, EP_OUT, 2, 64, 0, 0,
    7, USB_DESC_TYPE_ENDPOINT, EP_IN, 2, 64, 0, 0
};
static uint8_t device_desc[] = {
    18, USB_DESC_TYPE_DEVICE, 0, 2, 0, 0, 0, 64,
    LOBYTE(FIBRE_USB_VID), HIBYTE(FIBRE_USB_VID),
    LOBYTE(FIBRE_USB_PID), HIBYTE(FIBRE_USB_PID),
    0, 1, 1, 2, 3, 1
};
static uint8_t lang_desc[] = {4, USB_DESC_TYPE_STRING, 0x09, 0x04};
static uint8_t string_desc[USBD_MAX_STR_DESC_SIZ];
static uint8_t qualifier[] = {10, USB_DESC_TYPE_DEVICE_QUALIFIER, 0, 2, 0, 0, 0, 64, 1, 0};

static uint8_t* get_device(USBD_SpeedTypeDef speed, uint16_t* length) {
    *length = sizeof(device_desc); return device_desc;
}
static uint8_t* get_lang(USBD_SpeedTypeDef speed, uint16_t* length) {
    *length = sizeof(lang_desc); return lang_desc;
}
static uint8_t* get_manufacturer(USBD_SpeedTypeDef speed, uint16_t* length) {
    USBD_GetString((uint8_t*)"Fibre Example", string_desc, length); return string_desc;
}
static uint8_t* get_product(USBD_SpeedTypeDef speed, uint16_t* length) {
    USBD_GetString((uint8_t*)"Fibre USB Bulk", string_desc, length); return string_desc;
}
static uint8_t* get_serial(USBD_SpeedTypeDef speed, uint16_t* length) {
    static const char hex[] = "0123456789ABCDEF";
    char serial[25];
    uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
    for (unsigned i = 0; i < 24; ++i) serial[i] = hex[(uid[i / 8] >> (28 - 4 * (i % 8))) & 15];
    serial[24] = 0;
    USBD_GetString((uint8_t*)serial, string_desc, length); return string_desc;
}
static uint8_t* get_config(uint16_t* length) { *length = sizeof(config_desc); return config_desc; }
static uint8_t* get_qualifier(uint16_t* length) { *length = sizeof(qualifier); return qualifier; }

static uint8_t bulk_init(USBD_HandleTypeDef* dev, uint8_t config) {
    if (USBD_LL_OpenEP(dev, EP_OUT, USBD_EP_TYPE_BULK, 64) != USBD_OK ||
        USBD_LL_OpenEP(dev, EP_IN, USBD_EP_TYPE_BULK, 64) != USBD_OK) return USBD_FAIL;
    dev->ep_out[1].is_used = dev->ep_in[1].is_used = 1;
    dev->ep_out[1].maxpacket = dev->ep_in[1].maxpacket = 64;
    dev->pClassData = rx_buffer;
    dev->pClassDataCmsit[dev->classId] = rx_buffer;
    rx_length = tx_busy = tx_done = 0;
    ++generation;
    return USBD_LL_PrepareReceive(dev, EP_OUT, rx_buffer, sizeof(rx_buffer));
}
static uint8_t bulk_deinit(USBD_HandleTypeDef* dev, uint8_t config) {
    USBD_LL_CloseEP(dev, EP_OUT);
    USBD_LL_CloseEP(dev, EP_IN);
    dev->ep_out[1].is_used = dev->ep_in[1].is_used = 0;
    dev->pClassData = NULL;
    dev->pClassDataCmsit[dev->classId] = NULL;
    rx_length = tx_busy = tx_done = 0;
    ++generation;
    return USBD_OK;
}
static uint8_t bulk_setup(USBD_HandleTypeDef* dev, USBD_SetupReqTypedef* request) {
    static uint8_t zero[2];
    if ((request->bmRequest & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD && request->wIndex == 0) {
        switch (request->bRequest) {
        case USB_REQ_GET_STATUS: return USBD_CtlSendData(dev, zero, 2);
        case USB_REQ_GET_INTERFACE: return USBD_CtlSendData(dev, zero, 1);
        case USB_REQ_SET_INTERFACE: if (request->wValue == 0) return USBD_OK; break;
        default: break;
        }
    }
    USBD_CtlError(dev, request);
    return USBD_FAIL;
}
static uint8_t bulk_in(USBD_HandleTypeDef* dev, uint8_t endpoint) {
    tx_busy = 0;
    tx_done = 1;
    return USBD_OK;
}
static uint8_t bulk_out(USBD_HandleTypeDef* dev, uint8_t endpoint) {
    uint32_t count = USBD_LL_GetRxDataSize(dev, endpoint);
    if (count == 0 || count > FIBRE_USB_MTU) {
        return USBD_LL_PrepareReceive(dev, EP_OUT, rx_buffer, sizeof(rx_buffer));
    }
    // 主循环消费后再接收下一包；未消费时由 USB NAK 提供反压。
    __DMB();
    rx_length = count;
    return USBD_OK;
}
static USBD_DescriptorsTypeDef descriptors = {
    get_device, get_lang, get_manufacturer, get_product, get_serial, get_product, get_product
};
static USBD_ClassTypeDef bulk_class = {
    .Init = bulk_init, .DeInit = bulk_deinit, .Setup = bulk_setup,
    .DataIn = bulk_in, .DataOut = bulk_out,
    .GetHSConfigDescriptor = get_config, .GetFSConfigDescriptor = get_config,
    .GetOtherSpeedConfigDescriptor = get_config, .GetDeviceQualifierDescriptor = get_qualifier
};

void usb_bulk_init(void) {
    if (USBD_Init(&usb, &descriptors, 0) != USBD_OK ||
        USBD_RegisterClass(&usb, &bulk_class) != USBD_OK || USBD_Start(&usb) != USBD_OK) Error_Handler();
}
int usb_bulk_ready(void) { return usb.dev_state == USBD_STATE_CONFIGURED; }
uint32_t usb_bulk_generation(void) { return generation; }
int usb_bulk_read(uint8_t* buffer, uint32_t capacity) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    int count = (int)rx_length;
    if (!usb_bulk_ready()) count = 0;
    if (count) {
        if ((uint32_t)count > capacity) count = -1;
        else memcpy(buffer, rx_buffer, (size_t)count);
        rx_length = 0;
        USBD_LL_PrepareReceive(&usb, EP_OUT, rx_buffer, sizeof(rx_buffer));
    }
    __set_PRIMASK(mask);
    return count;
}
int usb_bulk_write(const uint8_t* buffer, uint32_t length) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    int result = -1;
    if (usb_bulk_ready() && !tx_busy && length > 0 && length <= FIBRE_USB_MTU) {
        memcpy(tx_buffer, buffer, length);
        tx_busy = 1;
        tx_done = 0;
        if (USBD_LL_Transmit(&usb, EP_IN, tx_buffer, length) == USBD_OK) result = 0;
        else tx_busy = 0;
    }
    __set_PRIMASK(mask);
    return result;
}
int usb_bulk_tx_completed(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    int done = tx_done;
    tx_done = 0;
    __set_PRIMASK(mask);
    return done;
}
