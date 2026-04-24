#pragma once
#include "LvglHal.hpp"

struct TouchCalStore {
    static bool load(TouchCalData *out);
    static bool save(const TouchCalData &c);
    static void clear();
    static bool is_sane(const TouchCalData &c);
};
