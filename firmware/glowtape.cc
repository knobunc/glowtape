#include <bit>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <pico/types.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"
#include "pico/aon_timer.h"
#include "pico/stdlib.h"

#include "font-tinytext.h"
#include "font-6x9.h"
#include "font-timetext.h"
#include "bdfont-support.h"

constexpr int kButtonPin = 4;

constexpr int kSpiTxPin = 11;  // TX1, 10=sck1, 9=CS1
constexpr uint16_t kFlashTimeMillis = 5;

enum class ScreenAspect {
  kAlongWidth,  // X axis along width; (0, 0) at top left after full pull.
  kAlongLength, // X axis along length; (0, 0) first in pull, at top.
};

// Sequence
//  StartNewImage();
//  SetPixel()...
//  for (SendStart(); SendNext(); /**/) {
//    WaitForSync();
//    LighFlash(flashmillis);
//  }
//
class FramePrinter {
  static constexpr int kMaxRows = 1024;
  static constexpr uint8_t kLightFlashPin = 8;
  static constexpr uint8_t kEvenOddLineOffset = 4;
public:
  using RowBits_t = uint64_t;
  FramePrinter(int spiTxPin, spi_inst_t *instance) : instance_(instance) {
    spi_init(instance_, 1'000'000);
    spi_set_format(instance_, 8,           // Regylar 8 bits transfer
                   spi_cpol_t::SPI_CPOL_1, // pos polarity
                   spi_cpha_t::SPI_CPHA_1, // phase
                   spi_order_t::SPI_MSB_FIRST);
    // Luckily, the RP2040 has pin-muxing pretty standardized and we can derive
    // the remaining pins from just knowing one pin.
    const int spiSckPin = spiTxPin - 1;
    const int spiCsPin = spiTxPin - 2;   // Also called 'latch' on glowxels
    gpio_set_function(spiTxPin, GPIO_FUNC_SPI);
    gpio_set_function(spiSckPin, GPIO_FUNC_SPI);
    gpio_set_function(spiCsPin, GPIO_FUNC_SPI);

    gpio_init(kLightFlashPin);
    gpio_set_dir(kLightFlashPin, GPIO_OUT);
    gpio_put(kLightFlashPin, true); // ~OE
  }

  // Start sending the new
  void SendStart() {
    send_pos_ = row_end_ - 1;
    if (send_pos_ >= kMaxRows - kEvenOddLineOffset) return;
    // We want to start the line offset earlier to cover all the bits.
    for (uint8_t i=0; i < kEvenOddLineOffset; ++i) {
      row_[++send_pos_] = 0;
    }
  }

  // Send next line. Can be done independently of actually flashing the light.
  // Returns 'true' if there is more to send.
  bool SendNext() {
    if (send_pos_ < 0) return false;
    printf("%d\n", send_pos_);
    SendData(assembleLedDataAt(send_pos_));
    --send_pos_;
    return true;
  }

  void LightFlash(uint16_t milliseconds) {
    gpio_put(kLightFlashPin, false); // ~OE
    sleep_ms(milliseconds);
    gpio_put(kLightFlashPin, true);
  }

  void StartNewImage(ScreenAspect type) {
    // Clear out previous image.
    for (int i = 0; i < row_end_; ++i) {
      row_[i] = 0;
    }
    aspect_type_ = type;
    row_end_ = 0;
  }

  // Set pixel on (x,y); interpreted in the context of Screenaspect
  void SetPixel(int x, int y, bool on = true) {
    constexpr int kMaxColumns = sizeof(RowBits_t) * 8;
    if (aspect_type_ == ScreenAspect::kAlongLength) {
      std::swap(x, y);
    }
    if (x < 0 || x >= kMaxColumns) {
      return;
    }

    RowBits_t &to_modify = at(y);
    constexpr RowBits_t kLeftMostBit = static_cast<RowBits_t>(1) << 63;
    if (on) {
      to_modify |= kLeftMostBit >> x;
    } else {
      to_modify &= ~(kLeftMostBit >> x);
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
    row_[row_end_++] = row_bits;
  }

  size_t size() const { return row_end_; }

  void WriteText(const struct FontData *font,
                 int xpos, int ypos, const char *print_text, bool right_aligned = false) {
    if (right_aligned) {
      for (const char *txt = print_text; *txt; ++txt) {
        auto g = bdfont_find_glyph(font, *txt);
        if (g) xpos -= g->width;
      }
    }
    for (const char *txt = print_text; *txt; ++txt) {
      int dx = 0;
      xpos += BDFONT_EMIT_GLYPH(font, *txt, false,
                                { dx = 0; },
                                {
                                  for (int i = 0; i < 8; ++i) {
                                    SetPixel(xpos+dx, stripe * 8 + ypos + i, b & (1 << i));
                                  }
                                  ++dx;
                                },
                                {});
    }
  }

private:
  void SendData(RowBits_t value) {
    // rp2040 stores things in LE, but we
    RowBits_t big_endian = std::byteswap(value);  // rp2040 is LE
    spi_write_blocking(instance_, (uint8_t*)&big_endian, sizeof(big_endian));
  }

  // Get column-offset and shift layoyt ready bits.
  RowBits_t assembleLedDataAt(int row) {
    return MapToPhysical(BitsAtRow(row));
  }

  RowBits_t BitsAtRow(int row) {
    // Even/Odd Pixels are interleaved 4 rows apart.
    RowBits_t result = row_[row] & 0x5555'5555'5555'5555;
    if (row >= 4) result |= row_[row - 4] & 0xAAAA'AAAA'AAAA'AAAA;
    return result;
  }

  // Physical mapping of 64 bits, mapped to the particular layout of the
  // bits in the four 16-bit shift register to LEDs they end up at.
  RowBits_t MapToPhysical(RowBits_t data) {
    return (static_cast<RowBits_t>(MapChipBits(data >> 48)) << 48) |
           (static_cast<RowBits_t>(MapChipBits(data >> 32)) << 32) |
           (static_cast<RowBits_t>(MapChipBits(data >> 16)) << 16) |
           (static_cast<RowBits_t>(MapChipBits(data >> 0)) << 0);
  }

  // Map data of one shift register chip to be from interleaved to the
  // corresponding bits on the top and bottom.
  constexpr uint16_t MapChipBits(uint16_t data) const {
    // -- Led mapping of bits in shift-register vs. position.
    constexpr uint8_t newpos[] = {
      7, 8, 6, 9, 5, 10, 4, 11, 3, 12, 2, 13, 1, 14, 0, 15
    };
    // There is probably a delightful hackers bit-fiddling that can do this,
    // but here pedestrian.
    uint16_t result = 0;
    for (uint8_t i = 0; i < 16; ++i) {
      if (data & (1 << i)) result |= (1 << newpos[i]);
    }
    return result;
  }

  RowBits_t row_[kMaxRows] = {0};
  int row_end_ = 0;  // like an end() iterator: the row beyond last.
  ScreenAspect aspect_type_ = ScreenAspect::kAlongWidth;

  int send_pos_ = -1;
  spi_inst_t *const instance_;
};

class StripEncoder {
  static constexpr int kLEDPin = 13;
  static constexpr int kLineEncoderIn = 7;
  static constexpr int64_t kTimeoutUsec = 500'000;
  static constexpr int64_t kFastTickUsec = 10'000;

public:
  enum class Result {
    kFirstTick,  // First tick after idle for a while
    kNoTick,     // No change detected
    kFastTick,   // Tick since last seen, but was very fast.
    kTick,       // change since last
  };

  StripEncoder() {
    gpio_init(kLineEncoderIn);
    gpio_set_dir(kLineEncoderIn, GPIO_IN);

    // Debug output.
    gpio_init(kLEDPin);
    gpio_set_dir(kLEDPin, GPIO_OUT);
  }

  // Needs to be called regularly.
  Result Poll() {
    const bool current_read = gpio_get(kLineEncoderIn);
    const bool edge_detected = current_read && !last_read_;
    last_read_ = current_read;

    if (!edge_detected) return Result::kNoTick;

    const absolute_time_t this_tick_time = get_absolute_time();
    const int64_t usec_diff = absolute_time_diff_us(last_tick_time_,
                                                    this_tick_time);
    last_tick_time_ = this_tick_time;

    if (usec_diff < kFastTickUsec) {
      gpio_put(kLEDPin, true);
      return Result::kFastTick;
    }
    gpio_put(kLEDPin, false);
    if (usec_diff > kTimeoutUsec) return Result::kFirstTick;
    return Result::kTick;
  }

private:
  bool last_read_ = false;
  absolute_time_t last_tick_time_{};
};


constexpr uint64_t operator""_bitmap(const char *str, size_t) {
  uint64_t result = 0;
  for (/**/; *str; ++str) {
    result |= (*str != ' ');
    result <<= static_cast<uint64_t>(1);
  }
  return result;
}

enum Content {
  kTime,
  kPicture,
};
void CreateContent(FramePrinter *out, Content c) {
  constexpr uint64_t kImage[] = {
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "       #####                                      #####         "_bitmap,
      "        ######                                  ######          "_bitmap,
      "         ######                                ######           "_bitmap,
      "           ####                                ####             "_bitmap,
      "           #####                              #####             "_bitmap,
      "          ######                              ######            "_bitmap,
      " #        ######                              ######        #   "_bitmap,
      " ##      #######                              #######      ##   "_bitmap,
      " ###    ########                              ########    ###   "_bitmap,
      " ####  ##########                            ##########  ####   "_bitmap,
      "  #################                        #################    "_bitmap,
      "  ##################                      ##################    "_bitmap,
      "   ##################       ######       ##################     "_bitmap,
      "    ##################   ############   ##################      "_bitmap,
      "      ###############  ################  ###############        "_bitmap,
      "            ########  ##################  ########              "_bitmap,
      "             ######  #################### #######               "_bitmap,
      "              ##### ###################### #####                "_bitmap,
      "               ###  ######################  ###                 "_bitmap,
      "                 # ######################## ##                  "_bitmap,
      "                  ##########################                    "_bitmap,
      "                  ##########################                    "_bitmap,
      "                  ##########################                    "_bitmap,
      "                 ############################                   "_bitmap,
      "                 ######    ########    ######                   "_bitmap,
      "                 #####      ######      #####                   "_bitmap,
      "                 ####        ####        ####                   "_bitmap,
      "                 ####       ######       ####                   "_bitmap,
      "                 ####      ########      ####                   "_bitmap,
      "                 ####    ############    ####                   "_bitmap,
      "                 ####   ##############   ####                   "_bitmap,
      "                 ##### ################ #####                   "_bitmap,
      "                 ############## #############                   "_bitmap,
      "                  ############  ############                    "_bitmap,
      "                  ############  ############                    "_bitmap,
      "               ##  ###########  ###########  ##                 "_bitmap,
      "              #### ######################## ####                "_bitmap,
      "             #####  ######################  #####               "_bitmap,
      "            #######  ####################  #######              "_bitmap,
      "      ############## #################### ##############        "_bitmap,
      "    #################  ################  #################      "_bitmap,
      "   ##################  ################  ##################     "_bitmap,
      "  ##################   ################   ##################    "_bitmap,
      "  ################     ##### #### #####     ################    "_bitmap,
      " ####  ##########       ###   ##   ###       ##########  ####   "_bitmap,
      " ###    ########                              ########    ###   "_bitmap,
      " ##      #######                              #######      ##   "_bitmap,
      " #        ######                              ######        #   "_bitmap,
      "           #####                              #####             "_bitmap,
      "           #####                              #####             "_bitmap,
      "          #####                                #####            "_bitmap,
      "         ######                                ######           "_bitmap,
      "        ######                                  ######          "_bitmap,
      "       #####                                      #####         "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
      "                                                                "_bitmap,
  };

  out->StartNewImage(ScreenAspect::kAlongWidth);

  switch (c) {
  case kTime:
    out->WriteText(&font_6x9, 62, 0, "Friday", true);
    out->WriteText(&font_6x9, 2, 10, "2024 10 31");
    out->WriteText(&font_timetext, 2, 22, "11:42");
    break;
  case kPicture:
    for (uint64_t row : kImage) {
      out->push_back(row);
    }
  }
}
int main() {
  stdio_init_all(); // Init serial, such as uart or usb

  gpio_set_dir(kButtonPin, GPIO_IN);

  StripEncoder encoder;
  FramePrinter printer(kSpiTxPin, spi1);  // 0.8mm * 1024 ≈ 82cm

  printer.SendStart();

  bool button_pressed = false;
  int16_t fast_steps = 0;
  for (;;) {
    button_pressed |= (!gpio_get(kButtonPin));  // Influence the next output.
    switch (encoder.Poll()) {
    case StripEncoder::Result::kFirstTick:
      CreateContent(&printer, button_pressed ? kPicture : kTime);
      printer.SendStart();
      fast_steps = 0;
      button_pressed = false;
      break;

    case StripEncoder::Result::kFastTick:
      ++fast_steps;
      [[fallthrough]];

    case StripEncoder::Result::kTick:
      if (printer.SendNext() && fast_steps < 4) {
        printer.LightFlash(kFlashTimeMillis);
      }
      break;

    case StripEncoder::Result::kNoTick:
      // Nothing to do.
      break;
    }
  }
}
