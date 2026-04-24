#include "TouchCal.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <app_config.h>

static bool read_blob(int32_t *x0, int32_t *x1, int32_t *y0, int32_t *y1) {
    Preferences p;
    if (!p.begin(APP_NVS_NAMESPACE, true))
        return false;
    uint32_t m = p.getUInt("m", 0);
    if (m != APP_NVS_CAL_MAGIC) {
        p.end();
        return false;
    }
    *x0 = p.getInt("x0", 0);
    *x1 = p.getInt("x1", 1);
    *y0 = p.getInt("y0", 0);
    *y1 = p.getInt("y1", 1);
    p.end();
    return true;
}

bool TouchCalStore::is_sane(const TouchCalData &c) {
    if (!c.valid)
        return false;
    if (c.xmax - c.xmin < 40)
        return false;
    if (c.ymax - c.ymin < 40)
        return false;
    return true;
}

bool TouchCalStore::load(TouchCalData *out) {
    int32_t x0, x1, y0, y1;
    if (!read_blob(&x0, &x1, &y0, &y1)) {
        out->valid = false;
        return false;
    }
    out->xmin  = x0;
    out->xmax  = x1;
    out->ymin  = y0;
    out->ymax  = y1;
    out->valid = true;
    if (!is_sane(*out)) {
        out->valid = false;
        return false;
    }
    return true;
}

bool TouchCalStore::save(const TouchCalData &c) {
    if (!is_sane(c))
        return false;
    Preferences p;
    if (!p.begin(APP_NVS_NAMESPACE, false))
        return false;
    p.putUInt("m", APP_NVS_CAL_MAGIC);
    p.putInt("x0", c.xmin);
    p.putInt("x1", c.xmax);
    p.putInt("y0", c.ymin);
    p.putInt("y1", c.ymax);
    p.end();
    return true;
}

void TouchCalStore::clear() {
    Preferences p;
    if (p.begin(APP_NVS_NAMESPACE, false)) {
        p.clear();
        p.end();
    }
}
