#pragma once
#include <stdint.h>
typedef void* osSemaphoreId;
typedef void* osMessageQId;
typedef void* osThreadId;
#ifdef __cplusplus
extern "C" {
#endif
int osMessagePut(osMessageQId queue, uint32_t event, uint32_t timeout);
#ifdef __cplusplus
}
#endif
