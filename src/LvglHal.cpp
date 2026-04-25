#include "LvglHal.hpp"
#include "Views.hpp"
#include "arcane_lvgl.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t s_mutex;
static TouchCalData      s_cal;
static bool              s_have_cal = false;

static void display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);
    M5.Display.writePixels((lgfx::swap565_t *)px_map, w * h);
    M5.Display.endWrite();
    lv_display_flush_ready(disp);
}

static void map_touch(int32_t *x, int32_t *y) {
    if (!s_have_cal || !s_cal.valid) {
        if (*x < 0)
            *x = 0;
        if (*x >= APP_LCD_WIDTH)
            *x = APP_LCD_WIDTH - 1;
        if (*y < 0)
            *y = 0;
        if (*y >= APP_LCD_HEIGHT)
            *y = APP_LCD_HEIGHT - 1;
        return;
    }
    int32_t rx = *x, ry = *y;
    if (s_cal.xmax > s_cal.xmin) {
        *x = (int32_t)((int64_t)(rx - s_cal.xmin) * (int64_t)(APP_LCD_WIDTH - 1) / (int64_t)(s_cal.xmax - s_cal.xmin));
    } else
        *x = 0;
    if (s_cal.ymax > s_cal.ymin) {
        *y = (int32_t)((int64_t)(ry - s_cal.ymin) * (int64_t)(APP_LCD_HEIGHT - 1) / (int64_t)(s_cal.ymax - s_cal.ymin));
    } else
        *y = 0;
    if (*x < 0)
        *x = 0;
    if (*x >= APP_LCD_WIDTH)
        *x = APP_LCD_WIDTH - 1;
    if (*y < 0)
        *y = 0;
    if (*y >= APP_LCD_HEIGHT)
        *y = APP_LCD_HEIGHT - 1;
}

static bool s_touch_down = false;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    lgfx::touch_point_t tp[1];
    M5.update();
    if (M5.Display.getTouch(tp, 1) > 0) {
        int32_t x = (int32_t)tp[0].x, y = (int32_t)tp[0].y;
        map_touch(&x, &y);
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = (lv_coord_t)x;
        data->point.y = (lv_coord_t)y;
        if (!s_touch_down) {
            s_touch_down = true;
            arc_notify_pointer_activity();
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        s_touch_down = false;
    }
}

static void tick_cb(void *arg) {
    (void)arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

void LvglHal::init() {
    s_mutex = xSemaphoreCreateMutex();
    s_cal   = {};
    s_have_cal = false;
    lv_init();

    const size_t buf_lines = 48;
    const size_t px        = (size_t)APP_LCD_WIDTH * buf_lines;
    void *buf1 = heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1) {
        buf1 = heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_8BIT);
    }
    if (!buf1) {
        Serial.println("LvglHal: out of memory for frame buffer");
        return;
    }
    const uint32_t buf_size = (uint32_t)(px * sizeof(lv_color_t));

    lv_display_t *disp = lv_display_create((int32_t)APP_LCD_WIDTH, (int32_t)APP_LCD_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, display_flush_cb);
    lv_display_set_buffers(disp, (uint8_t *)buf1, nullptr, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_dpi(disp, 200);
    lv_display_set_default(disp);

    lv_display_set_theme(
        disp,
        lv_theme_default_init(disp, lv_color_hex(APP_C_ACCENT), lv_color_hex(0x1E1E24), true, &lv_font_montserrat_18));

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    const esp_timer_create_args_t targs = {.callback = &tick_cb, .name = "lv_tick"};
    esp_timer_handle_t tmr;
    if (esp_timer_create(&targs, &tmr) == ESP_OK) {
        esp_timer_start_periodic(tmr, (uint64_t)LV_TICK_PERIOD_MS * 1000u);
    }
}

void LvglHal::lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void LvglHal::unlock() { xSemaphoreGive(s_mutex); }

void LvglHal::set_touch_cal(const TouchCalData &c) {
    s_cal      = c;
    s_have_cal = c.valid;
}
void LvglHal::get_touch_cal(TouchCalData *out) {
    *out = s_cal;
}

void LvglHal::click_feedback() {
#if !defined(DISABLE_SPK_FEEDBACK)
    M5.Speaker.tone(1600.0f, 14, -1, true);
#endif
}
