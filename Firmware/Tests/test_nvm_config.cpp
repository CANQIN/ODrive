#include <doctest.h>
#include <array>
#include <cstring>
#include <vector>
#include "MotorControl/nvm_config.hpp"

namespace {
std::vector<uint8_t> nvm_image;
}

extern "C" {
int NVM_init(void) { return 0; }
size_t NVM_get_max_write_length(void) { return 256; }
int NVM_start_write(size_t size) { nvm_image.assign(size, 0xff); return 0; }
int NVM_commit(void) { return 0; }
int NVM_read(size_t offset, uint8_t* data, size_t size) {
    if (offset > nvm_image.size() || size > nvm_image.size() - offset) { return -1; }
    std::memcpy(data, nvm_image.data() + offset, size);
    return 0;
}
int NVM_write(size_t offset, uint8_t* data, size_t size) {
    if (offset > nvm_image.size() || size > nvm_image.size() - offset) { return -1; }
    std::memcpy(nvm_image.data() + offset, data, size);
    return 0;
}
}

TEST_CASE("ConfigManager rejects version 1 even when the stored size is unchanged") {
    std::array<uint8_t, 8> payload{0, 1, 0, 1, 2, 3, 4, 5};
    const uint16_t old_crc = calc_crc16<CONFIG_CRC16_POLYNOMIAL>(
        CONFIG_CRC16_INIT ^ 0x0001, payload.data(), payload.size());
    nvm_image.assign(payload.begin(), payload.end());
    nvm_image.push_back(static_cast<uint8_t>(old_crc));
    nvm_image.push_back(static_cast<uint8_t>(old_crc >> 8));

    ConfigManager manager;
    std::array<uint8_t, 8> loaded{};
    REQUIRE(manager.start_load());
    REQUIRE(manager.read(&loaded));
    CHECK(loaded == payload);
    CHECK_FALSE(manager.finish_load(nullptr));
}

TEST_CASE("ConfigManager saves and reloads the current configuration format") {
    std::array<uint8_t, 8> payload{5, 4, 3, 2, 1, 0, 1, 0};
    ConfigManager writer;
    REQUIRE(writer.prepare_store());
    REQUIRE(writer.write(&payload));
    size_t occupied = 0;
    REQUIRE(writer.start_store(&occupied));
    REQUIRE(writer.write(&payload));
    REQUIRE(writer.finish_store());
    CHECK(occupied == payload.size() + 2);

    ConfigManager reader;
    std::array<uint8_t, 8> loaded{};
    REQUIRE(reader.start_load());
    REQUIRE(reader.read(&loaded));
    REQUIRE(reader.finish_load(nullptr));
    CHECK(loaded == payload);
}
