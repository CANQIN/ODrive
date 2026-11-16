#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_NO_WINDOWS_SEH
#include <doctest.h>
#include <vector>
#include <cstring>
#include <cstdlib>

// Windows SDK annotations collide with ST's endpoint-direction enum.
#undef IN
#undef OUT

extern "C" {
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "cmsis_os.h"
char serial_number_str[13] = "123456789ABC";
osMessageQId usb_event_queue = nullptr;
void* USBD_static_malloc(uint32_t size) { return std::malloc(size); }
void USBD_static_free(void* data) { std::free(data); }
}

namespace {
PCD_HandleTypeDef pcd{};
std::vector<uint8_t> transmitted;
std::vector<uint32_t> events;
uint8_t transmit_ep = 0;
unsigned stalls = 0;
unsigned received_ep = 0;
unsigned received_size = 0;
bool fifos_configured = false;
USBD_StatusTypeDef ll_init_status = USBD_OK;

int8_t init() { return 0; }
int8_t deinit() { return 0; }
int8_t control(uint8_t, uint8_t*, uint16_t) { return 0; }
int8_t receive(uint8_t*, uint32_t* size, uint8_t ep) {
    received_ep = ep;
    received_size = *size;
    return 0;
}
USBD_CDC_ItfTypeDef interface_callbacks{init, deinit, control, receive, nullptr};

struct Device {
    USBD_HandleTypeDef usb{};
    Device() {
        stalls = received_ep = received_size = 0;
        transmitted.clear();
        events.clear();
        REQUIRE(USBD_Init(&usb, &FS_Desc, 0) == USBD_OK);
        REQUIRE(USBD_RegisterClass(&usb, &USBD_CDC) == USBD_OK);
        REQUIRE(USBD_CDC_RegisterInterface(&usb, &interface_callbacks) == USBD_OK);
        REQUIRE(USBD_LL_SetSpeed(&usb, USBD_SPEED_FULL) == USBD_OK);
        REQUIRE(USBD_LL_Reset(&usb) == USBD_OK);
    }
    ~Device() { USBD_DeInit(&usb); }
    void setup(uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length) {
        uint8_t bytes[] = {type, request, uint8_t(value), uint8_t(value >> 8),
                          uint8_t(index), uint8_t(index >> 8), uint8_t(length), uint8_t(length >> 8)};
        REQUIRE(USBD_LL_SetupStage(&usb, bytes) == USBD_OK);
    }
    void configure() {
        setup(0, USB_REQ_SET_ADDRESS, 1, 0, 0);
        setup(0, USB_REQ_SET_CONFIGURATION, 1, 0, 0);
        REQUIRE(usb.dev_state == USBD_STATE_CONFIGURED);
        REQUIRE(usb.pClassData != nullptr);
    }
};
}

extern "C" {
int osMessagePut(osMessageQId, uint32_t event, uint32_t) { events.push_back(event); return 0; }
USBD_StatusTypeDef odrive_cubemx_usb_ll_init(USBD_HandleTypeDef* dev) {
    fifos_configured = false;
    dev->pData = &pcd;
    return ll_init_status;
}
void odrive_usb_configure_fifos(PCD_HandleTypeDef* handle) {
    CHECK(handle == &pcd);
    fifos_configured = true;
}
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef*) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef*) {
    CHECK(fifos_configured);
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef*) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef*, uint8_t ep, uint8_t, uint16_t size) {
    if (ep & 0x80) pcd.IN_ep[ep & 0xf].maxpacket = size;
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef*, uint8_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef*, uint8_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef*, uint8_t) { ++stalls; return USBD_OK; }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef*, uint8_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef*, uint8_t) { return USBD_OK; }
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef*, uint8_t ep, uint8_t* data, uint32_t size) {
    transmit_ep = ep;
    transmitted.clear();
    if (size) transmitted.assign(data, data + size);
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef*, uint8_t, uint8_t*, uint32_t) { return USBD_OK; }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef*, uint8_t) { return 0; }
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef*, uint8_t) { return 17; }
void USBD_LL_Delay(uint32_t) {}
}

TEST_CASE("USB configures native endpoint FIFO after LL init and before connecting") {
    Device device;
    CHECK(fifos_configured);
    CHECK(USBD_Start(&device.usb) == USBD_OK);
    ll_init_status = USBD_FAIL;
    CHECK(USBD_LL_Init(&device.usb) == USBD_FAIL);
    CHECK_FALSE(fifos_configured);
    ll_init_status = USBD_OK;
}

TEST_CASE("Cube USB enumerates CDC and native interfaces with original IDs") {
    Device device;
    device.setup(0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_DEVICE << 8, 0, 18);
    REQUIRE(transmitted.size() == 18);
    CHECK(transmitted[8] == 0x09); CHECK(transmitted[9] == 0x12);
    CHECK(transmitted[10] == 0x32); CHECK(transmitted[11] == 0x0d);
    device.setup(0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_CONFIGURATION << 8, 0, 255);
    REQUIRE(transmitted.size() >= 9);
    CHECK(transmitted[4] == 3);
    CHECK((transmitted[2] | (transmitted[3] << 8)) == transmitted.size());
    std::vector<uint8_t> endpoints;
    for (size_t pos = 0; pos < transmitted.size(); pos += transmitted[pos]) {
        REQUIRE(transmitted[pos] >= 2);
        REQUIRE(pos + transmitted[pos] <= transmitted.size());
        if (transmitted[pos + 1] == USB_DESC_TYPE_ENDPOINT) endpoints.push_back(transmitted[pos + 2]);
    }
    CHECK(endpoints == std::vector<uint8_t>{CDC_CMD_EP, CDC_OUT_EP, CDC_IN_EP, ODRIVE_OUT_EP, ODRIVE_IN_EP});
    device.configure();
    CHECK(device.usb.pClassData == device.usb.pClassDataCmsit[0]);
    device.setup(0x81, USB_REQ_GET_INTERFACE, 0, 2, 1);
    CHECK(transmitted == std::vector<uint8_t>{0});
    CHECK(stalls == 0);
}

TEST_CASE("Windows can retrieve the MS OS string and WinUSB compatibility descriptor") {
    Device device;
    device.setup(0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_TYPE_STRING << 8) | 0xee, 0, 255);
    REQUIRE(transmitted.size() == 18);
    CHECK(transmitted[2] == 'M'); CHECK(transmitted[16] == MS_VendorCode);
    device.setup(0xc0, MS_VendorCode, 0, 4, 40);
    REQUIRE(transmitted.size() == 40);
    CHECK(transmitted[16] == 2);
    CHECK(std::memcmp(transmitted.data() + 18, "WINUSB", 6) == 0);
    CHECK(stalls == 0);
}

TEST_CASE("Both bulk endpoints complete full packets with ZLP and dispatch receives") {
    Device device;
    device.configure();
    uint8_t data[64]{};
    for (uint8_t ep : {uint8_t(CDC_IN_EP), uint8_t(ODRIVE_IN_EP)}) {
        REQUIRE(USBD_CDC_TransmitPacket(&device.usb, data, sizeof(data), ep) == USBD_OK);
        CHECK(transmit_ep == ep);
        CHECK(USBD_CDC_TransmitPacket(&device.usb, data, sizeof(data), ep) == USBD_BUSY);
        REQUIRE(USBD_LL_DataInStage(&device.usb, ep & 0xf, data) == USBD_OK);
        CHECK(transmitted.empty());
        CHECK(USBD_CDC_TransmitPacket(&device.usb, data, sizeof(data), ep) == USBD_BUSY);
        REQUIRE(USBD_LL_DataInStage(&device.usb, ep & 0xf, nullptr) == USBD_OK);
        REQUIRE(USBD_CDC_TransmitPacket(&device.usb, data, 17, ep) == USBD_OK);
        REQUIRE(USBD_LL_DataInStage(&device.usb, ep & 0xf, data) == USBD_OK);
        CHECK(events.back() == (ep == CDC_IN_EP ? 3 : 4));
        REQUIRE(USBD_LL_DataOutStage(&device.usb, ep & 0xf, data) == USBD_OK);
        CHECK(received_ep == (ep & 0xf)); CHECK(received_size == 17);
    }
    REQUIRE(USBD_ClrClassConfig(&device.usb, 1) == USBD_OK);
    CHECK(device.usb.pClassData == nullptr);
    CHECK(device.usb.pClassDataCmsit[0] == nullptr);
}
