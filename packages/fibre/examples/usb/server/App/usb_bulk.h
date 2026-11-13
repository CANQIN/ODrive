#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// 每个 Fibre 包小于 USB FS 的 64 字节包长，单包传输不需要尾部 ZLP。
#define FIBRE_USB_MTU 63U
#define FIBRE_USB_VID 0xCAFEU
#define FIBRE_USB_PID 0xF405U
void usb_bulk_init(void);
int usb_bulk_ready(void);
uint32_t usb_bulk_generation(void);
int usb_bulk_read(uint8_t* buffer, uint32_t capacity);
int usb_bulk_write(const uint8_t* buffer, uint32_t length);
int usb_bulk_tx_completed(void);

#ifdef __cplusplus
}
#endif
