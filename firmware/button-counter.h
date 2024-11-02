#ifndef BUTTON_COUNTER_H
#define BUTTON_COUNTER_H

#include "hardware/gpio.h"

#include "pico/time.h"

class ButtonCounter {
  static constexpr int kDebounceTimeUsec = 100'000;
 public:
  ButtonCounter(int pin) : pin_(pin) { gpio_set_dir(pin_, GPIO_IN); }

  void Poll() {
    const bool is_pressed = !gpio_get(pin_);
    const absolute_time_t this_press_time = get_absolute_time();
    const int64_t usec_diff =
        absolute_time_diff_us(last_press_time_, this_press_time);

    if (!previous_pressed_ && is_pressed && usec_diff > kDebounceTimeUsec) {
      ++count_;
      last_press_time_ = this_press_time;
    }
    previous_pressed_ = is_pressed;
  }

  void Reset() {
    previous_pressed_ = 0;
    count_ = 0;
  }

  int count() const { return count_; }

 private:
  const int pin_;
  int count_ = 0;
  bool previous_pressed_ = 0;
  absolute_time_t last_press_time_{};
};

#endif
