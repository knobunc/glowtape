#include <bit>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

constexpr int kRotationEncoderSDAPin = 26;
constexpr int kSpiTxPin = 7;

enum class ScreenAspect {
  kAlongWidth,  // X axis along width; (0, 0) at top left after full pull.
  kAlongLength, // X axis along length; (0, 0) first in pull, at top.
};

template <typename RowBits_t, int kMaxRows>
class FramePrinter {
  static constexpr uint8_t kStrobePin = 3;
public:
  FramePrinter(int spiTxPin, spi_inst_t *instance) : instance_(instance) {
    spi_init(instance_, 1'000'000);
    spi_set_format(instance_, 8,           // Regylar 8 bits transfer
                   spi_cpol_t::SPI_CPOL_1, // pos polarity
                   spi_cpha_t::SPI_CPHA_1, // phase
                   spi_order_t::SPI_MSB_FIRST);
    // Luckily, the RP2040 has pin-muxing pretty standardized and we can derive
    // the remaining pins from just one pin.
    const int spiSckPin = spiTxPin - 1;
    const int spiCsPin = spiTxPin - 2;
    gpio_set_function(spiTxPin, GPIO_FUNC_SPI);
    gpio_set_function(spiSckPin, GPIO_FUNC_SPI);
    gpio_set_function(spiCsPin, GPIO_FUNC_SPI);

    gpio_init(kStrobePin);
    gpio_set_dir(kStrobePin, GPIO_OUT);
    gpio_put(kStrobePin, true); // ~OE
  }

  void SendLine(RowBits_t value) {
    // rp2040 stores things in LE, but we
    RowBits_t big_endian = std::byteswap(value);  // rp2040 is LE
    spi_write_blocking(instance_, (uint8_t*)&big_endian, sizeof(big_endian));
  }

  void Strobe(uint16_t milliseconds) {
    gpio_put(kStrobePin, false); // ~OE
    sleep_ms(milliseconds);
    gpio_put(kStrobePin, true);
  }

  void StartNewImage(ScreenAspect type) {
    // Clear out previous image.
    for (int i = 0; i < row_end_; ++i) {
      row_[i] = 0;
    }
    aspect_type_ = type;
    row_end_ = 0;
  }

  void SetPixel(int x, int y, bool on = true) {
    constexpr int kMaxColumns = sizeof(RowBits_t) * 8;
    if (aspect_type_ == ScreenAspect::kAlongLength) {
      std::swap(x, y);
    }
    if (x < 0 || x >= kMaxColumns) {
      return;
    }

    RowBits_t &to_modify = at(y);
    if (on) {
      to_modify |= static_cast<RowBits_t>(1) << x;
    } else {
      to_modify &= ~(static_cast<RowBits_t>(1) << x);
    }
  }

  // Provides access to the given row, possibly expanding the current image.
  RowBits_t &at(int r) {
    if (r < 0 || r >= kMaxRows) return row_[kMaxRows - 1];  // error fallback
    row_end_ = (r >= row_end_) ? (r + 1) : row_end_;  // implicit last
    return row_[r];
  }

  void push_back(RowBits_t row_bits) {
    if (row_end_ >= kMaxRows) return;
    row_[++row_end_] = row_bits;
  }

  size_t size() const { return row_end_; }

private:
  RowBits_t row_[kMaxRows] = {0};
  int row_end_ = 0;  // like an end() iterator: the row beyond last.
  ScreenAspect aspect_type_ = ScreenAspect::kAlongWidth;
  spi_inst_t *const instance_;
};

class RotationalEncoder {
  static constexpr uint8_t kAS5600Addr = 0x36;
  static constexpr uint8_t kRawAngleAddress = 0x0c; // and 0x0d

public:
  RotationalEncoder(int pinSDA, i2c_inst_t *instance)
    : instance_(instance) {
    gpio_set_function(pinSDA, GPIO_FUNC_I2C);
    gpio_set_function(pinSDA + 1, GPIO_FUNC_I2C);  // SCL is always offset + 1
    i2c_init(instance_, 400000);
  }

  uint16_t Value() {
    uint8_t raw[2];
    i2c_write_blocking(instance_, kAS5600Addr, &kRawAngleAddress, 1, false);
    i2c_read_blocking(instance_, kAS5600Addr, raw, 2, false);
    return (raw[0] << 8) | raw[1];
  }

private:
  i2c_inst_t *const instance_;
};

constexpr uint16_t operator""_bitmap(const char *str, size_t len) {
  uint16_t result = 0;
  for (/**/; *str; ++str) {
    result |= (*str != ' ');
    result <<= 1;
  }
  return result;
}

int main() {
  stdio_init_all(); // Init serial, such as uart or usb
  RotationalEncoder encoder(kRotationEncoderSDAPin, i2c1);
  FramePrinter<uint16_t, 1024> printer(kSpiTxPin, spi0);  // 0.8mm * 1024 ≈ 82cm

  printer.StartNewImage(ScreenAspect::kAlongWidth);
  constexpr uint16_t kImage[] = {
    "#      #      "_bitmap,
    "#      #      "_bitmap,
    "#      #      "_bitmap,
    "#      #      "_bitmap,
    "############# "_bitmap,
    "#      #   #  "_bitmap,
    "#      #  #   "_bitmap,
    "#      # #    "_bitmap,
    "#      ###### "_bitmap,
    "              "_bitmap,
    "# # # # # # # "_bitmap,
  };

  for (uint16_t row : kImage) {
    printer.push_back(row);
  }
  printer.at(0x0f) = 0;

  uint16_t last = 0;
  for (uint8_t i;/**/;++i) {
    const uint16_t value = encoder.Value();
    const uint16_t diff = (value - last) & 0x0FFF;
    last = value;
    printf("Encoder-value %d diff: %d\n", value, diff);
    printer.SendLine(printer.at(i % 16));
    printer.Strobe(5);
    sleep_ms(100);
  }
}
