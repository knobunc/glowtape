#ifndef STRIP_ENCODER_H
#define STRIP_ENCODER_H

#include <cstdint>

#include "hardware/gpio.h"
#include "pico/time.h"

class StripEncoder {
  static constexpr int kLEDPin = 13;
  static constexpr int kLineEncoderIn = 7;
  static constexpr int64_t kTimeoutUsec = 500'000;
  static constexpr int64_t kFastTickUsec = 7'000;

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
    const int64_t usec_diff =
        absolute_time_diff_us(last_tick_time_, this_tick_time);
    last_tick_time_ = this_tick_time;

    if (usec_diff < kFastTickUsec) {
      gpio_put(kLEDPin, true);
      return Result::kFastTick;
    }
    gpio_put(kLEDPin, false);
    if (usec_diff > kTimeoutUsec) {
      return Result::kFirstTick;
    }
    return Result::kTick;
  }

 private:
  bool last_read_ = false;
  absolute_time_t last_tick_time_{};
};
#endif
