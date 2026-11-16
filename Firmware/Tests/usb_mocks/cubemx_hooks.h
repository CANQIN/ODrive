#pragma once
#include "stm32f4xx_hal.h"

// Hardware FIFO allocation is supplied by the selected board in firmware.
void odrive_usb_configure_fifos(PCD_HandleTypeDef* pcd);
