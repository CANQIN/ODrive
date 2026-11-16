"""Validate electrical/timing invariants after CubeMX regeneration (no hardware)."""
import ast
import operator
import re
from pathlib import Path

BOARD = Path(__file__).resolve().parent
CUBE = BOARD / "CubeMX"

def read(path):
    return path.read_text(encoding="utf-8")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def expression(text, names):
    """Evaluate integer C timer arithmetic without eval or floating point."""
    operators = {ast.Add: operator.add, ast.Sub: operator.sub,
                 ast.Mult: operator.mul, ast.Div: operator.floordiv}

    def visit(node):
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return names[node.id]
        if isinstance(node, ast.BinOp) and type(node.op) in operators:
            return operators[type(node.op)](visit(node.left), visit(node.right))
        raise ValueError(f"Unsupported timer expression: {text}")

    return visit(ast.parse(text, mode="eval").body)


def main():
    ioc = dict(line.split("=", 1) for line in read(CUBE / "CubeMX.ioc").splitlines()
               if "=" in line and not line.startswith("#"))
    for key, value in {"MxCube.Version": "6.18.1", "Mcu.CPN": "STM32F405RGT6",
                       "ProjectManager.FirmwarePackage": "STM32Cube FW_F4 V1.28.3",
                       "ProjectManager.KeepUserCode": "true", "ProjectManager.LibraryCopy": "0"}.items():
        require(ioc.get(key) == value, f"Unexpected {key}: {ioc.get(key)}")

    constants = {k: int(v) for k, v in re.findall(r"#define\s+(TIM_\w+)\s+(\d+)",
                                                read(CUBE / "Core/Inc/main.h"))}
    timers = read(CUBE / "Core/Src/tim.c")
    for timer, expected in ((1, 3500), (8, 3500), (2, 4096), (13, 10499)):
        value = re.search(rf"htim{timer}\.Init.Period = ([^;]+);", timers).group(1)
        require(expression(value, constants) == expected, f"TIM{timer} period changed: {value}")
    require(expression(ioc["TIM13.Period"], constants) == 10499, "IOC TIM13 period drift")
    for timer in (1, 8):
        body = timers.split(f"void MX_TIM{timer}_Init(void)", 1)[1].split("/* TIM", 1)[0]
        require(f"htim{timer}.Init.RepetitionCounter = TIM_1_8_RCR;" in body, "PWM repetition changed")
        require("DeadTime = TIM_1_8_DEADTIME_CLOCKS;" in body, "PWM dead time changed")
    require(constants["TIM_1_8_RCR"] == 2 and constants["TIM_1_8_DEADTIME_CLOCKS"] == 20,
            "PWM timing constants changed")

    dma = read(CUBE / "Core/Src/dma.c")
    for stream, priority in ((0, 4), (2, 10), (4, 10), (5, 10), (6, 10), (7, 3)):
        require(f"HAL_NVIC_SetPriority(DMA1_Stream{stream}_IRQn, {priority}, 0);" in dma,
                f"DMA1 stream {stream} priority changed")
    require("HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn)" not in dma, "Polling ADC must not enable DMA IRQ")
    require("HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn)" not in timers, "Unused TIM1 IRQ enabled")
    require("htim5.Init.Period = 0xFFFFFFFF;" in timers and
            "HAL_TIM_Base_Init(&htim5)" in timers, "PWM input capture time base changed")
    for irq, priority in (("TIM5_IRQn", 1), ("TIM8_UP_TIM13_IRQn", 0)):
        require(f"HAL_NVIC_SetPriority({irq}, {priority}, 0);" in timers, f"{irq} priority changed")
    spi = read(CUBE / "Core/Src/spi.c")
    require("odrive_spi_dma_init(spiHandle);" in spi, "Runtime SPI DMA alignment hook lost")
    for direction, priority in (("tx", "HIGH"), ("rx", "MEDIUM")):
        require(f"hdma_spi3_{direction}.Init.Priority = DMA_PRIORITY_{priority};" in spi,
                "SPI TX must win DMA arbitration over RX")

    config = read(CUBE / "Core/Inc/FreeRTOSConfig.h")
    for key, value in (("configTOTAL_HEAP_SIZE", "65536"), ("configSUPPORT_STATIC_ALLOCATION", "0"),
                       ("configAPPLICATION_ALLOCATED_HEAP", "1"),
                       ("configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY", "5")):
        require(re.search(rf"#define\s+{key}\s+(?:\(\(size_t\))?{value}\b", config), f"{key} changed")
    require("#undef xPortSysTickHandler" in config, "ODrive SysTick forwarding lost")

    can = read(CUBE / "Core/Src/can.c")
    for field, value in (("Prescaler", "8"), ("SyncJumpWidth", "CAN_SJW_4TQ"),
                         ("TimeSeg1", "CAN_BS1_16TQ"), ("TimeSeg2", "CAN_BS2_4TQ"),
                         ("AutoBusOff", "ENABLE"), ("AutoWakeUp", "ENABLE"),
                         ("AutoRetransmission", "ENABLE")):
        require(f"hcan1.Init.{field} = {value};" in can, f"CAN {field} changed")
    uart = read(CUBE / "Core/Src/usart.c")
    for name, instance, rx, tx in (("uart4", "UART4", 2, 4), ("usart2", "USART2", 5, 6)):
        require(f"hdma_{name}_rx.Instance = DMA1_Stream{rx};" in uart and
                f"hdma_{name}_tx.Instance = DMA1_Stream{tx};" in uart, f"{name} DMA routing changed")
        require(f"HAL_NVIC_SetPriority({instance}_IRQn, 10, 0);" in uart, f"{name} IRQ priority changed")
    require(not any(value.startswith("I2C") for key, value in ioc.items() if re.fullmatch(r"Mcu.IP\d+", key)),
            "Removed bus must not be enabled in CubeMX")
    require(not re.search(r"^\s*#define\s+HAL_I2C_MODULE_ENABLED\b",
                          read(CUBE / "Core/Inc/stm32f4xx_hal_conf.h"), re.M),
            "Removed HAL module must stay disabled")
    require(not list((CUBE / "Drivers/STM32F4xx_HAL_Driver").rglob("*i2c*")),
            "Unused two-wire drivers were not pruned")
    interrupts = read(CUBE / "Core/Src/stm32f4xx_it.c")
    for vector in ("TIM5", "TIM8_UP_TIM13", "OTG_HS", "I2C1_EV", "I2C1_ER"):
        require(f"void {vector}_IRQHandler(void)" not in interrupts,
                f"Unexpected generated vector: {vector}")
    for vector in ("HardFault", "SysTick"):
        require(f"void {vector}_Handler(void)" not in interrupts, f"Duplicate platform vector: {vector}")
    for vector in ("CAN1_TX", "CAN1_RX0", "CAN1_RX1", "CAN1_SCE", "USART2", "UART4", "OTG_FS"):
        require(f"void {vector}_IRQHandler(void)" in interrupts, f"Missing generated IRQ: {vector}")
    require("COUNT_IRQ(W1_" not in interrupts, "Invalid fault-loop IRQ counter")
    usb = read(CUBE / "USB_DEVICE/Target/usbd_conf.h")
    require(re.search(r"#define\s+USBD_MAX_NUM_INTERFACES\s+3U", usb) and
            "#define USBD_SUPPORT_USER_STRING_DESC 1" in usb, "USB composite/WinUSB configuration changed")
    require(re.search(r"#define\s+TICK_INT_PRIORITY\s+6U", read(CUBE / "Core/Inc/stm32f4xx_hal_conf.h")),
            "HAL tick priority changed")
    linker = read(BOARD / "STM32F405RGTx_FLASH.ld")
    nvm = read(BOARD / "Inc/nvm_config.h")
    for sector, address, index in (("A", "080C0000", 10), ("B", "080E0000", 11)):
        require(f"#define ODRIVE_NVM_SECTOR_{sector} FLASH_SECTOR_{index}" in nvm and
                f"0x{address}UL" in nvm and
                f"#define ODRIVE_NVM_SECTOR_{sector}_SIZE 0x20000UL" in nvm,
                f"NVM sector {sector} no longer matches the linker reservation")
    for region, origin, size in (("FLASH", "0x8000000", "768K"), ("NVM", "0x80C0000", "256K"),
                                 ("CCMRAM", "0x10000000", "64K")):
        require(re.search(rf"{region}\s+\([^)]*\)\s*:\s*ORIGIN = {origin}, LENGTH = {size}", linker),
                f"{region} layout changed")
    main_source = read(CUBE / "Core/Src/main.c")
    for assignment in ("PLLM = 4", "PLLN = 168", "PLLP = RCC_PLLP_DIV2", "PLLQ = 7",
                       "SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK", "AHBCLKDivider = RCC_SYSCLK_DIV1",
                       "APB1CLKDivider = RCC_HCLK_DIV4", "APB2CLKDivider = RCC_HCLK_DIV2"):
        require(assignment + ";" in main_source, f"System clock changed: {assignment}")
    require(re.search(r"#define\s+HSE_VALUE\s+8000000U", read(CUBE / "Core/Inc/stm32f4xx_hal_conf.h")),
            "8 MHz crystal configuration changed")
    for timer in (1, 8):
        require(f"TIM{timer}->BDTR &= ~(TIM_BDTR_AOE | TIM_BDTR_MOE);" in main_source,
                "Error_Handler must disable PWM outputs")
    print("CubeMX checks passed: v3.6-56V, PWM, CAN/UART, DMA/vector ownership, USB, RTOS and NVM.")


if __name__ == "__main__":
    main()
