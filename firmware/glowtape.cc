#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <pico/types.h>

#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "bdfont-support.h"
#include "font-6x9.h"
#include "font-timetext.h"
#include "font-tinytext.h"

constexpr int kButtonPin = 4;

constexpr int kSpiTxPin = 11; // TX1, 10=sck1, 9=CS1
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
    const int spiCsPin = spiTxPin - 2; // Also called 'latch' on glowxels
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
    if (send_pos_ >= kMaxRows - kEvenOddLineOffset)
      return;
    // We want to start the line offset earlier to cover all the bits.
    for (uint8_t i = 0; i < kEvenOddLineOffset; ++i) {
      row_[++send_pos_] = 0;
    }
  }

  // Send next line. Can be done independently of actually flashing the light.
  // Returns 'true' if there is more to send.
  bool SendNext() {
    if (send_pos_ < 0)
      return false;
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
    if (r < 0 || r >= kMaxRows)
      return row_[kMaxRows - 1];                     // error fallback
    row_end_ = (r >= row_end_) ? (r + 1) : row_end_; // implicit last
    return row_[r];
  }

  void push_back(RowBits_t row_bits) {
    if (row_end_ >= kMaxRows)
      return;
    row_[row_end_++] = row_bits;
  }

  size_t size() const { return row_end_; }

  void WriteText(const struct FontData *font, int xpos, int ypos,
                 const char *print_text, bool right_aligned = false) {
    if (right_aligned) {
      for (const char *txt = print_text; *txt; ++txt) {
        auto g = bdfont_find_glyph(font, *txt);
        if (g)
          xpos -= g->width;
      }
    }
    for (const char *txt = print_text; *txt; ++txt) {
      int dx = 0;
      xpos += BDFONT_EMIT_GLYPH(
          font, *txt, false, { dx = 0; },
          {
            for (int i = 0; i < 8; ++i) {
              SetPixel(xpos + dx, stripe * 8 + ypos + i, b & (1 << i));
            }
            ++dx;
          },
          {});
    }
  }

private:
  void SendData(RowBits_t value) {
    // rp2040 stores things in LE, but we
    RowBits_t big_endian = std::byteswap(value); // rp2040 is LE
    spi_write_blocking(instance_, (uint8_t *)&big_endian, sizeof(big_endian));
  }

  // Get column-offset and shift layoyt ready bits.
  RowBits_t assembleLedDataAt(int row) { return MapToPhysical(BitsAtRow(row)); }

  RowBits_t BitsAtRow(int row) {
    // Even/Odd Pixels are interleaved 4 rows apart.
    RowBits_t result = row_[row] & 0x5555'5555'5555'5555;
    if (row >= 4)
      result |= row_[row - 4] & 0xAAAA'AAAA'AAAA'AAAA;
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
    constexpr uint8_t newpos[] = {7, 8,  6, 9,  5, 10, 4, 11,
                                  3, 12, 2, 13, 1, 14, 0, 15};
    // There is probably a delightful hackers bit-fiddling that can do this,
    // but here pedestrian.
    uint16_t result = 0;
    for (uint8_t i = 0; i < 16; ++i) {
      if (data & (1 << i))
        result |= (1 << newpos[i]);
    }
    return result;
  }

  RowBits_t row_[kMaxRows] = {0};
  int row_end_ = 0; // like an end() iterator: the row beyond last.
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
    kFirstTick, // First tick after idle for a while
    kNoTick,    // No change detected
    kFastTick,  // Tick since last seen, but was very fast.
    kTick,      // change since last
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

    if (!edge_detected)
      return Result::kNoTick;

    const absolute_time_t this_tick_time = get_absolute_time();
    const int64_t usec_diff =
        absolute_time_diff_us(last_tick_time_, this_tick_time);
    last_tick_time_ = this_tick_time;

    if (usec_diff < kFastTickUsec) {
      gpio_put(kLEDPin, true);
      return Result::kFastTick;
    }
    gpio_put(kLEDPin, false);
    if (usec_diff > kTimeoutUsec)
      return Result::kFirstTick;
    return Result::kTick;
  }

private:
  bool last_read_ = false;
  absolute_time_t last_tick_time_{};
};

template <size_t kBufferSize> class LineReader {
  using LineProcessor = std::function<void(const char *line, size_t len)>;

public:
  LineReader(const LineProcessor &line_processor)
      : pos_(buf_), end_(buf_ + sizeof(buf_)), callback_(line_processor) {}

  void Poll() {
    const int maybe_char = getchar_timeout_us(0);
    if (maybe_char < 0)
      return;
    putchar(maybe_char);
    *pos_++ = (char)maybe_char;
    const bool is_eol = (maybe_char == '\r' || maybe_char == '\n');
    if (is_eol || pos_ >= end_) {
      if (is_eol) {
        *(pos_ - 1) = '\0'; // Don't want newline.
      } else {
        *pos_ = '\0';
      }
      callback_(buf_, pos_ - buf_);
      pos_ = buf_;
    }
  }

private:
  char buf_[kBufferSize];
  char *pos_;
  const char *const end_;
  LineProcessor callback_;
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

  if (c == kPicture) {
    for (uint64_t row : kImage) {
      out->push_back(row);
    }
    return;
  }

  datetime_t now{};
  const bool time_valid = rtc_get_datetime(&now);
  if (!time_valid) {
    out->WriteText(&font_6x9, 2, 0, "Set Time!");
    return;
  }
  const char *weekday = "-";
  switch (now.dotw) {
  case 0:
    weekday = "Sun";
    break;
  case 1:
    weekday = "Mon";
    break;
  case 2:
    weekday = "Tue";
    break;
  case 3:
    weekday = "Wed";
    break;
  case 4:
    weekday = "Thu";
    break;
  case 5:
    weekday = "Fri";
    break;
  case 6:
    weekday = "Sat";
    break;
  }
  out->WriteText(&font_6x9, 62, 0, weekday, true);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%4d %02d %02d", now.year, now.month,
           now.day);
  out->WriteText(&font_6x9, 2, 10, buffer);

  snprintf(buffer, sizeof(buffer), "%02d:%02d", now.hour, now.min);
  out->WriteText(&font_timetext, 2, 22, buffer);
}

static void TimeSetter(const char *value, size_t) {
  int year, month, day, hour, min, sec, day_of_week;
  int count = sscanf(value, "%d-%d-%d %d:%d:%d %d\n", &year, &month, &day,
                     &hour, &min, &sec, &day_of_week);
  if (count != 7) {
    printf("Error: expected YYYY-MM-DD hh:mm:ss dow\n"
           "With dow: 0=SUN 1=MON 2=TUE 3=WED 4=THU 5=FRI 6=SUN\n");
    return;
  }

  const datetime_t now = {
      .year = (int16_t)year,
      .month = (int8_t)month,
      .day = (int8_t)day,
      .dotw = (int8_t)day_of_week,
      .hour = (int8_t)hour,
      .min = (int8_t)min,
      .sec = (int8_t)sec,
  };
  if (rtc_set_datetime(&now)) {
    printf("\nOK\n");
  } else {
    printf("\nERROR\n");
  }
}

int main() {
  stdio_init_all(); // Init serial, such as uart or usb

  rtc_init();

  gpio_set_dir(kButtonPin, GPIO_IN);

  LineReader<256> process_serial(&TimeSetter);
  StripEncoder encoder;
  FramePrinter printer(kSpiTxPin, spi1); // 0.8mm * 1024 ≈ 82cm

  printer.SendStart();

  bool button_pressed = false;
  int16_t fast_steps = 0;
  for (;;) {
    process_serial.Poll();
    button_pressed |= (!gpio_get(kButtonPin)); // Influence the next output.
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
