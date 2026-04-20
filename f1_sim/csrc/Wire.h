#pragma once
// Wire (I2C) stub for desktop / WASM simulation.
// Provides just enough of the Wire API to compile ht16k33_display.h.

#include <cstdint>
#include <cstring>

// Buffer to capture HT16K33 display data for inspection
constexpr uint8_t SIM_I2C_BUF_SIZE = 17;  // 1 register byte + 16 data bytes max
extern uint8_t sim_i2c_buf[SIM_I2C_BUF_SIZE];
extern uint8_t sim_i2c_buf_len;
extern uint8_t sim_i2c_display_buf[16];  // captured display RAM (written when register 0x00)

class TwoWire {
public:
    void begin() {}

    void beginTransmission(uint8_t) {
        buf_pos_ = 0;
    }

    void write(uint8_t data) {
        if (buf_pos_ < SIM_I2C_BUF_SIZE) {
            local_buf_[buf_pos_++] = data;
        }
    }

    uint8_t endTransmission() {
        // If first byte is 0x00 (display RAM register), capture remaining bytes
        if (buf_pos_ > 1 && local_buf_[0] == 0x00) {
            uint8_t count = buf_pos_ - 1;
            if (count > 16) count = 16;
            memcpy(sim_i2c_display_buf, local_buf_ + 1, count);
        }
        // Copy to global buffer for inspection
        sim_i2c_buf_len = buf_pos_;
        if (buf_pos_ > 0) {
            memcpy(sim_i2c_buf, local_buf_, buf_pos_ < SIM_I2C_BUF_SIZE ? buf_pos_ : SIM_I2C_BUF_SIZE);
        }
        return 0;
    }

private:
    uint8_t local_buf_[SIM_I2C_BUF_SIZE] = {};
    uint8_t buf_pos_ = 0;
};

extern TwoWire Wire;

