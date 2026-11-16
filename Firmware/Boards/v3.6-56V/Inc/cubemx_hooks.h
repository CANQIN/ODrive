#pragma once
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPI transactions switch between 8-bit and 16-bit encoder/driver protocols.
void odrive_spi_dma_init(SPI_HandleTypeDef* spi);
void odrive_usb_configure_fifos(PCD_HandleTypeDef* pcd);
void odrive_fault_shutdown(void);

#ifdef __cplusplus
}
#endif
