#include <pico/types.h>

#include <cstdio>

#include "bdfont-support.h"
#include "button-counter.h"
#include "frame-printer.h"
#include "hardware/rtc.h"
#include "line-reader.h"
#include "strip-encoder.h"

// Generated font data
#include "font-6x9.h"
#include "font-large.h"
#include "font-message.h"
#include "font-timetext.h"

// Some demo bitmap content
#include "bitmap-contents.h"

// Time a single line is flashing the 405nm LEDs.
constexpr uint16_t kFlashTimeMillis = 5;

// GPIO where the input button lives. The Feathre board has the BOOTSEL button
/// connected to this GPIO.
constexpr int kButtonPin = 4;

constexpr int kSpiTxPin = 11;  // TX1, 10=sck1, 9=CS1

// A simple wrapper to emit bdfont text to out FramePrinter.
static void WriteText(FramePrinter *out, const struct FontData *font, int xpos,
                      int ypos, const char *print_text,
                      bool right_aligned = false, int extra_space = 0) {
  if (right_aligned) {
    for (const char *txt = print_text; *txt; ++txt) {
      auto g = bdfont_find_glyph(font, *txt);
      if (g) xpos -= g->width;
      xpos -= extra_space;
    }
  }
  for (const char *txt = print_text; *txt; ++txt) {
    int dx = 0;
    xpos += BDFONT_EMIT_GLYPH(
        font, *txt, false, { dx = 0; },
        {
          for (int i = 0; i < 8; ++i) {
            out->SetPixel(xpos + dx, stripe * 8 + ypos + i, b & (1 << i));
          }
          ++dx;
        },
        {});
    xpos += extra_space;
  }
}

void DrawTime(FramePrinter *out) {
  datetime_t now{};
  const bool time_valid = rtc_get_datetime(&now);
  if (!time_valid) {
    // RTC of rp2040 is a bit annoying, its content does not survive
    // resets. To be set with the set-time.sh script.
    WriteText(out, &font_6x9, 2, 0, "Set Time!");
    return;
  }
  const char *weekday = "-";
  switch (now.dotw) {
    case 0: weekday = "Sun"; break;
    case 1: weekday = "Mon"; break;
    case 2: weekday = "Tue"; break;
    case 3: weekday = "Wed"; break;
    case 4: weekday = "Thu"; break;
    case 5: weekday = "Fri"; break;
    case 6: weekday = "Sat"; break;
  }
  WriteText(out, &font_6x9, 62, 0, weekday, true);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%4d %02d %02d", now.year, now.month,
           now.day);
  WriteText(out, &font_6x9, 2, 10, buffer);

  snprintf(buffer, sizeof(buffer), "%02d:%02d", now.hour, now.min);
  WriteText(out, &font_timetext, 2, 22, buffer);
}

// Create content, derived from number of button presses before.
void CreateContent(FramePrinter *out, int what_content) {
  enum Content {
    kTime,   // Default: no button presses
    kName,
    kJollyWrencher,
    kSupercon,
    kProject,
  };

  out->StartNewImage(ScreenAspect::kAlongWidth);

  if (what_content == kJollyWrencher) {
    for (uint64_t row : kJollyWrencherBitmap) {
      out->push_back(row);
    }
    return;
  }

  if (what_content == kName) {
    WriteText(out, &font_message, 2, 0, "Henner", false, 2);
    WriteText(out, &font_message, 62, 15, "Zeller", true, 2);
    return;
  }

  if (what_content == kSupercon) {
    out->StartNewImage(ScreenAspect::kAlongLength);
    WriteText(out, &font_large, 0, 8, "Supercon 8");
    return;
  }

  if (what_content == kProject) {
    for (uint64_t row : kProjectQRBitmap) {
      out->push_back(row);
    }
    return;
  }

  // None of the above ? Ok, time then.
  DrawTime(out);
}

static void TimeSetter(const char *value) {
  int year, month, day, hour, min, sec, day_of_week;
  int count = sscanf(value, "%d-%d-%d %d:%d:%d %d\n", &year, &month, &day,
                     &hour, &min, &sec, &day_of_week);
  if (count != 7) {
    printf(
        "Error: expected YYYY-MM-DD hh:mm:ss dow\n"
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
  stdio_init_all();  // Init serial, such as uart or usb
  rtc_init();

  // Peripherals
  LineReader<256> process_serial(&TimeSetter);
  StripEncoder encoder;
  ButtonCounter button(kButtonPin);
  FramePrinter printer(kSpiTxPin, spi1);  // 0.8mm * 1024 ≈ 82cm

  int16_t fast_steps = 0;
  int16_t forward_steps = 0;
  for (;;) {
    process_serial.Poll();
    button.Poll();

    switch (encoder.Poll()) {
      case StripEncoder::Result::kFirstTick:
        CreateContent(&printer, button.count());
        printer.SendStart();
        fast_steps = forward_steps = 0;
        button.Reset();
        break;

      case StripEncoder::Result::kFastTick:
        ++fast_steps;  // If we see more than 4 of these, stop sending anything.
        if (forward_steps > 4 && fast_steps < 4 && printer.SendNext()) {
          printer.LightFlash(kFlashTimeMillis);
        }
        break;

      case StripEncoder::Result::kTick:
        ++forward_steps;
        if (forward_steps > 4 && fast_steps < 4 && printer.SendNext()) {
          printer.LightFlash(kFlashTimeMillis);
        }
        break;

      case StripEncoder::Result::kNoTick:
        // Nothing to do.
        break;
    }
  }
}
