#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include "display_cfg.h"
#include "data.h"
#include "ui.h"
#include "ble.h"
#include "power.h"
#include "imu.h"
#include "splash.h"
#include "usage_rate.h"
#include "touch_tt21100.h"

// Physical inputs on ESP32-S3-BOX:
//   BTN_BACK (GPIO 0, BOOT momentary)  — cycle screen / advance splash
//   BTN_FWD  (GPIO 1, mute slider)     — sends Shift+Tab on each flip
#define BTN_BACK 0
#define BTN_FWD  1

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, -1 /* MISO */, FSPI);
// ILI9342C (native landscape, IPS, inverted colors).
Arduino_GFX *gfx = new Arduino_ILI9342(
    bus, LCD_RESET, 0 /* rotation 0 = native 320x240 */, false /* ips */);

static UsageData usage = {};

// ---- Touch interrupt + shared state ----
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;
static volatile bool     touch_data_ready = false;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

static void touch_read() {
    // Poll TT21100 directly at ~30Hz. INT line on this board variant
    // doesn't fire reliably, so we ask for a frame on a timer.
    static uint32_t last_ms = 0;
    static uint32_t last_ok_ms = 0;
    uint32_t now = millis();
    if (now - last_ms < 30) return;
    last_ms = now;

    uint16_t tx, ty;
    if (tt21100_read(&tx, &ty)) {
        last_ok_ms = now;
        touch_pressed = true;
        touch_x = tx;
        touch_y = ty;
    } else if (now - last_ok_ms > 80) {
        touch_pressed = false;
    }
}

// ---- LVGL draw buffers (PSRAM-backed, partial render) ----
#define BUF_LINES 40
static uint16_t *buf1 = nullptr;
static uint16_t *buf2 = nullptr;

static uint32_t my_tick(void) {
    return millis();
}

static void my_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    uint16_t *src = (uint16_t*)px_map;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, src, w, h);
    lv_display_flush_ready(disp);
}

static void my_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    if (touch_pressed) {
        data->point.x = touch_x;
        data->point.y = touch_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static bool parse_json(const char* json, UsageData* out) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    out->session_pct = doc["s"] | 0.0f;
    out->session_reset_mins = doc["sr"] | -1;
    out->weekly_pct = doc["w"] | 0.0f;
    out->weekly_reset_mins = doc["wr"] | -1;
    strlcpy(out->status, doc["st"] | "unknown", sizeof(out->status));
    out->ok = doc["ok"] | false;
    out->valid = true;
    return true;
}

#define CMD_BUF_SIZE 64
static char cmd_buf[CMD_BUF_SIZE];
static int cmd_pos = 0;

static void send_screenshot() {
    const uint32_t w = LCD_WIDTH, h = LCD_HEIGHT;
    const uint32_t row_bytes = w * 2;
    const uint32_t buf_size = row_bytes * h;
    uint8_t* sbuf = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!sbuf) {
        Serial.println("SCREENSHOT_ERR");
        return;
    }

    lv_draw_buf_t draw_buf;
    lv_draw_buf_init(&draw_buf, w, h, LV_COLOR_FORMAT_RGB565, row_bytes, sbuf, buf_size);

    lv_result_t res = lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &draw_buf);
    if (res != LV_RESULT_OK) {
        heap_caps_free(sbuf);
        Serial.println("SCREENSHOT_ERR");
        return;
    }

    Serial.printf("SCREENSHOT_START %lu %lu %lu\n", (unsigned long)w, (unsigned long)h, (unsigned long)buf_size);
    Serial.flush();
    Serial.write(sbuf, buf_size);
    Serial.flush();
    Serial.println();
    Serial.println("SCREENSHOT_END");

    heap_caps_free(sbuf);
}

static void check_serial_cmd() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            cmd_buf[cmd_pos] = '\0';
            if (strcmp(cmd_buf, "screenshot") == 0) {
                send_screenshot();
            }
            cmd_pos = 0;
        } else if (cmd_pos < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_pos++] = c;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("{\"ready\":true}");

    // Probe raw I2C line levels BEFORE driver init.
    pinMode(IIC_SDA, INPUT);
    pinMode(IIC_SCL, INPUT);
    delay(5);
    int sda_pre = digitalRead(IIC_SDA);
    int scl_pre = digitalRead(IIC_SCL);
    Serial.printf("I2C-PROBE-PRE: SDA(GPIO%d)=%d SCL(GPIO%d)=%d\n",
                  IIC_SDA, sda_pre, IIC_SCL, scl_pre);

    Wire.begin(IIC_SDA, IIC_SCL);

    delay(20);
    pinMode(IIC_SDA, INPUT);
    pinMode(IIC_SCL, INPUT);
    delay(5);
    int sda_post = digitalRead(IIC_SDA);
    int scl_post = digitalRead(IIC_SCL);
    Serial.printf("I2C-PROBE-POST: SDA=%d SCL=%d (1,1=ok; 0,1=slave holds SDA; 1,0=SCL stuck; 0,0=short/no pullups)\n",
                  sda_post, scl_post);
    Wire.begin(IIC_SDA, IIC_SCL);

    // Init display
    gfx->begin();
    gfx->fillScreen(0x0000);
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    power_init();
    imu_init();

    // Init touch — TT21100 shares RST with LCD; gfx->begin() pulsed it, wait
    // for the controller to come back up before probing.
    delay(150);
    if (!tt21100_init()) {
        Serial.println("Touch init failed");
    } else {
        pinMode(TP_INT, INPUT_PULLUP);
        attachInterrupt(TP_INT, touch_isr, CHANGE);
    }

    lv_init();
    lv_tick_set_cb(my_tick);

    buf1 = (uint16_t*)heap_caps_malloc(LCD_WIDTH * BUF_LINES * 2, MALLOC_CAP_SPIRAM);
    buf2 = (uint16_t*)heap_caps_malloc(LCD_WIDTH * BUF_LINES * 2, MALLOC_CAP_SPIRAM);

    lv_display_t* disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, my_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, LCD_WIDTH * BUF_LINES * 2,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_cb);

    ble_init();

    pinMode(BTN_BACK, INPUT_PULLUP);
    pinMode(BTN_FWD,  INPUT_PULLUP);

    ui_init();
    ui_update_ble_status(ble_get_state(), ble_get_device_name(), ble_get_mac_address());
    ui_update_battery(power_battery_pct(), power_is_charging());
    ui_show_screen(SCREEN_SPLASH);

    Serial.println("Dashboard ready, waiting for data on BLE...");
}

static ble_state_t last_ble_state = BLE_STATE_INIT;

void loop() {
    touch_read();
    lv_timer_handler();
    ui_tick_anim();
    ble_tick();
    power_tick();
    imu_tick();
    splash_tick();

    // Physical inputs: BOOT cycles screens, slider sends Shift+Tab on flip.
    {
        static bool back_was = false, fwd_was = false;
        bool back_now = (digitalRead(BTN_BACK) == LOW);
        bool fwd_now  = (digitalRead(BTN_FWD)  == LOW);

        if (back_now != back_was) {
            if (back_now) {
                if (ui_get_current_screen() == SCREEN_SPLASH) ui_show_screen(SCREEN_USAGE);
                else                                          ui_cycle_screen();
            }
            back_was = back_now;
        }
        if (fwd_now != fwd_was) {
            ble_keyboard_press(0x2B, 0x02);
            delay(10);
            ble_keyboard_release();
            fwd_was = fwd_now;
        }
    }

    ble_state_t bs = ble_get_state();
    if (bs != last_ble_state) {
        last_ble_state = bs;
        ui_update_ble_status(bs, ble_get_device_name(), ble_get_mac_address());
    }

    static int last_pct = -2;
    static bool last_charging = false;
    int pct = power_battery_pct();
    bool charging = power_is_charging();
    if (pct != last_pct || charging != last_charging) {
        last_pct = pct;
        last_charging = charging;
        ui_update_battery(pct, charging);
    }

    check_serial_cmd();

    if (ble_has_data()) {
        if (parse_json(ble_get_data(), &usage)) {
            int g_before = usage_rate_group();
            usage_rate_sample(usage.session_pct);
            int g_after = usage_rate_group();
            if (g_after != g_before) {
                Serial.printf("usage rate: group %d -> %d (s=%.2f%%)\n",
                    g_before, g_after, usage.session_pct);
                if (splash_is_active()) splash_pick_for_current_rate();
            }
            ui_update(&usage);
            ble_send_ack();
        } else {
            ble_send_nack();
        }
    }

    delay(5);
}
