#ifndef BUTTON_COUNTER_H
#define BUTTON_COUNTER_H

#include "hardware/gpio.h"

class ButtonCounter {
 public:
  ButtonCounter(int pin) : pin_(pin) { gpio_set_dir(pin_, GPIO_IN); }

  void Poll() {
    const bool is_pressed = !gpio_get(pin_);
    if (!previous_pressed_ && is_pressed) {
      ++count_;
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
};

#endif
