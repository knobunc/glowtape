#ifndef LINE_READER_H
#define LINE_READER_H

#include <cstdio>
#include <functional>

#include "pico/stdlib.h"

template <size_t kBufferSize>
class LineReader {
  using LineProcessor = std::function<void(const char *line, size_t len)>;

 public:
  LineReader(const LineProcessor &line_processor)
      : pos_(buf_), end_(buf_ + sizeof(buf_)), callback_(line_processor) {}

  void Poll() {
    const int maybe_char = getchar_timeout_us(0);
    if (maybe_char < 0) return;
    putchar(maybe_char);
    *pos_++ = (char)maybe_char;
    const bool is_eol = (maybe_char == '\r' || maybe_char == '\n');
    if (is_eol || pos_ >= end_) {
      if (is_eol) {
        *(pos_ - 1) = '\0';  // Don't want newline.
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

#endif  // LINE_READER_H
