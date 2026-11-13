#include "usbd_core.h"
#include "usb_otg.h"

// CubeMX 负责 PCD 初始化；这里把 ST USB Device Core 接到该 PCD 实例。
static USBD_StatusTypeDef status(HAL_StatusTypeDef result) {
    return result == HAL_OK ? USBD_OK : USBD_FAIL;
}
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* dev) {
    dev->pData = &hpcd_USB_OTG_FS;
    hpcd_USB_OTG_FS.pData = dev;
    if (HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 128) != HAL_OK ||
        HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 64) != HAL_OK ||
        HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 128) != HAL_OK) return USBD_FAIL;
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef* dev) { return status(HAL_PCD_DeInit(dev->pData)); }
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef* dev) { return status(HAL_PCD_Start(dev->pData)); }
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef* dev) { return status(HAL_PCD_Stop(dev->pData)); }
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef* dev, uint8_t ep, uint8_t type, uint16_t size) {
    return status(HAL_PCD_EP_Open(dev->pData, ep, size, type));
}
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* dev, uint8_t ep) { return status(HAL_PCD_EP_Close(dev->pData, ep)); }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* dev, uint8_t ep) { return status(HAL_PCD_EP_Flush(dev->pData, ep)); }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* dev, uint8_t ep) { return status(HAL_PCD_EP_SetStall(dev->pData, ep)); }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* dev, uint8_t ep) { return status(HAL_PCD_EP_ClrStall(dev->pData, ep)); }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef* dev, uint8_t ep) {
    PCD_HandleTypeDef* pcd = dev->pData;
    return ep & 0x80 ? pcd->IN_ep[ep & 0x7F].is_stall : pcd->OUT_ep[ep].is_stall;
}
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef* dev, uint8_t address) {
    return status(HAL_PCD_SetAddress(dev->pData, address));
}
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* dev, uint8_t ep, uint8_t* data, uint32_t length) {
    return status(HAL_PCD_EP_Transmit(dev->pData, ep, data, length));
}
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef* dev, uint8_t ep, uint8_t* data, uint32_t length) {
    return status(HAL_PCD_EP_Receive(dev->pData, ep, data, length));
}
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef* dev, uint8_t ep) { return HAL_PCD_EP_GetRxCount(dev->pData, ep); }
void USBD_LL_Delay(uint32_t delay) { HAL_Delay(delay); }

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* pcd) { USBD_LL_SetupStage(pcd->pData, (uint8_t*)pcd->Setup); }
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* pcd, uint8_t ep) { USBD_LL_DataOutStage(pcd->pData, ep, pcd->OUT_ep[ep].xfer_buff); }
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* pcd, uint8_t ep) { USBD_LL_DataInStage(pcd->pData, ep, pcd->IN_ep[ep].xfer_buff); }
void HAL_PCD_ResetCallback(PCD_HandleTypeDef* pcd) {
    USBD_LL_SetSpeed(pcd->pData, USBD_SPEED_FULL);
    USBD_LL_Reset(pcd->pData);
}
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* pcd) { USBD_LL_Suspend(pcd->pData); }
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* pcd) { USBD_LL_Resume(pcd->pData); }
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* pcd) { USBD_LL_DevConnected(pcd->pData); }
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* pcd) { USBD_LL_DevDisconnected(pcd->pData); }
