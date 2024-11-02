#ifndef BUTTON_COUNTER_H
#define BUTTON_COUNTER_H

#include "hardware/gpio.h"
#include "pico/time.h"

class ButtonCounter {
  static constexpr int kDebounceTimeUsec = 50'000;

  enum State {
    kIdle,
    kStartPress,
    kPressed,
    kStartRelease,
  };

 public:
  ButtonCounter(int pin) : pin_(pin) {
    gpio_set_dir(pin_, GPIO_IN);
    Reset();
  }

  void Poll() {
    const absolute_time_t now = get_absolute_time();
    const bool is_pressed = !gpio_get(pin_);  // Board-Button has inverted logic
    switch (state_) {
      case kIdle:
        if (is_pressed) {
          SetState(kStartPress, now);
        }
        break;
      case kStartPress:
        if (!is_pressed) {
          SetState(kIdle, now);  // fallback
        } else if (StateAge(now) > kDebounceTimeUsec) {
          ++count_;
          SetState(kPressed, now);
        }
        break;
      case kPressed:
        if (!is_pressed) {
          SetState(kStartRelease, now);
        }
        break;
      case kStartRelease:
        if (is_pressed) {
          SetState(kPressed, now);  // Glitch back
        } else if (StateAge(now) > kDebounceTimeUsec) {
          SetState(kIdle, now);
        }
    }
  }

  void Reset() {
    state_ = kIdle;
    count_ = 0;
  }

  int count() const { return count_; }

 private:
  void SetState(State s, absolute_time_t now) {
    if (s == state_) return;
    state_ = s;
    state_change_time_ = now;
  }

  int64_t StateAge(absolute_time_t now) const {
    return absolute_time_diff_us(state_change_time_, now);
  }

  const int pin_;
  State state_;
  int count_ = 0;
  absolute_time_t state_change_time_{};
};

#endif
