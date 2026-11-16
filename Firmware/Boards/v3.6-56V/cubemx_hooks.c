#include "cubemx_hooks.h"
#include "main.h"

void odrive_fault_shutdown(void) {
    TIM1->BDTR &= ~(TIM_BDTR_AOE | TIM_BDTR_MOE);
    TIM8->BDTR &= ~(TIM_BDTR_AOE | TIM_BDTR_MOE);
}

void odrive_usb_configure_fifos(PCD_HandleTypeDef* pcd) {
    // F405 OTG FS has 320 FIFO words. Allocate CDC, notification and native IN
    // endpoints (128 + 32 + 64 + 16 + 64 = 304 words) before USBD_Start.
    HAL_PCDEx_SetRxFiFo(pcd, 128);
    HAL_PCDEx_SetTxFiFo(pcd, 0, 32);
    HAL_PCDEx_SetTxFiFo(pcd, 1, 64);
    HAL_PCDEx_SetTxFiFo(pcd, 2, 16);
    HAL_PCDEx_SetTxFiFo(pcd, 3, 64);
}

void odrive_spi_dma_init(SPI_HandleTypeDef* spi) {
    const uint32_t peripheral_alignment = spi->Init.DataSize == SPI_DATASIZE_8BIT
        ? DMA_PDATAALIGN_BYTE : DMA_PDATAALIGN_HALFWORD;
    const uint32_t memory_alignment = spi->Init.DataSize == SPI_DATASIZE_8BIT
        ? DMA_MDATAALIGN_BYTE : DMA_MDATAALIGN_HALFWORD;
    spi->hdmatx->Init.PeriphDataAlignment = peripheral_alignment;
    spi->hdmatx->Init.MemDataAlignment = memory_alignment;
    spi->hdmarx->Init.PeriphDataAlignment = peripheral_alignment;
    spi->hdmarx->Init.MemDataAlignment = memory_alignment;
    if (HAL_DMA_Init(spi->hdmatx) != HAL_OK || HAL_DMA_Init(spi->hdmarx) != HAL_OK) {
        Error_Handler();
    }
}
