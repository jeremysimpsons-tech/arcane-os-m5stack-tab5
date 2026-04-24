#pragma once
#include "app_config.h"
#include <cstdint>

struct TouchCalData {
    int32_t xmin, xmax, ymin, ymax;
    bool    valid{false};
};

class LvglHal {
  public:
    static void init();
    static void lock();
    static void unlock();
    static void set_touch_cal(const TouchCalData &c);
    static void get_touch_cal(TouchCalData *out);
    /** Short tap feedback (speaker if available). */
    static void click_feedback();
};
