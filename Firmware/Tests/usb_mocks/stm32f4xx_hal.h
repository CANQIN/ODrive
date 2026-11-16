#pragma once
#include "stm32f4xx.h"

// Only the endpoint packet size is consumed by the real USB class driver.
typedef struct { uint32_t maxpacket; } USB_TestEndpoint;
typedef struct { USB_TestEndpoint IN_ep[16]; } PCD_HandleTypeDef;
