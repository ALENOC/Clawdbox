#include <Arduino.h>
#include <lvgl.h>
#include "display_cfg.h"
#include "data.h"
#include "ui.h"
#include "power.h"
#include "splash.h"
#include "usage_rate.h"
#include "touch.h"
#include "wifi_cfg.h"
#include "net.h"
#include "poll.h"
#include "pair.h"
#include "backlight.h"
#include "settings_mgr.h"
#include "tz_auto.h"
#include <time.h>

// Physical inputs on ESP32-S3-BOX variants:
//   BTN_BACK (GPIO 0, BOOT momentary)  — cycle screen / advance splash;
//                                        long-press (>5s) → clear NVS + reboot
//   BTN_FWD  (GPIO 1, mute slider)     — manual poll trigger
#define BTN_BACK 0
#define BTN_FWD  1
#define BTN_LONG_PRESS_MS 5000

// bus and gfx are allocated in setup() after board detection.
Arduino_DataBus *bus = nullptr;
Arduino_GFX     *gfx = nullptr;

static UsageData usage = {};

// ---- Touch shared state ----
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;
static volatile bool     touch_data_ready = false;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

static void poll_touch() {
    // Poll at ~30Hz. INT line doesn't fire reliably on all BOX variants,
    // so we ask for a frame on a timer.
    static uint32_t last_ms = 0;
    static uint32_t last_ok_ms = 0;
    uint32_t now = millis();
    if (now - last_ms < 30) return;
    last_ms = now;

    uint16_t tx, ty;
    if (touch_read(&tx, &ty)) {
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
            } else if (strcmp(cmd_buf, "clearauth") == 0) {
                Serial.println("clearauth: wiping tokens, rebooting");
                cfg_clear_tokens();
                delay(200);
                ESP.restart();
            }
            cmd_pos = 0;
        } else if (cmd_pos < CMD_BUF_SIZE - 1) {
            cmd_buf[cmd_pos++] = c;
        }
    }
}

void setup() {
    settings_init();
    bl_init(settings_get()->brightness);

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

    // Probe touch controller type — BOX-3 uses result to select display driver.
    touch_probe();

    // Allocate display bus (SPI, pins common across all BOX variants).
    bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, -1 /* MISO */, FSPI);

    // Select display driver based on board target.
    // BOX-3: GT911 touch → ILI9342C panel; TT21100 (or none) → ST7789 panel.
#if defined(BOARD_S3BOX)
    gfx = new Arduino_ILI9342(bus, LCD_RESET, 0 /* native landscape */, false /* ips */);
    Serial.println("display: ILI9342C (S3BOX)");
#elif defined(BOARD_S3BOX_LITE)
    gfx = new Arduino_ST7789(bus, LCD_RESET, 1 /* rotate 90° */, true /* ips */);
    Serial.println("display: ST7789 (S3BOX-Lite)");
#elif defined(BOARD_S3BOX_3)
    if (touch_hw_type() == TOUCH_TYPE_GT911) {
        gfx = new Arduino_ILI9342(bus, LCD_RESET, 0, false);
        Serial.println("display: ILI9342C (S3BOX-3, GT911 panel)");
    } else {
        gfx = new Arduino_ST7789(bus, LCD_RESET, 1, true);
        Serial.println("display: ST7789 (S3BOX-3, TT21100 panel)");
    }
#else
    #error "No board target defined."
#endif

    // Init display (backlight stays OFF until first LVGL frame is painted).
    gfx->begin();
    gfx->fillScreen(0x0000);

    power_init();

    // Init touch — TT21100/GT911 share RST with LCD; gfx->begin() pulsed it,
    // wait for the controller to come back up before probing.
#if BOARD_HAS_TOUCH
    delay(150);
    if (!touch_init()) {
        Serial.println("touch_init failed");
    } else {
        pinMode(TP_INT, INPUT_PULLUP);
        attachInterrupt(TP_INT, touch_isr, CHANGE);
    }
#endif

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

    cfg_init();
    net_init();
    poll_init();

    pinMode(BTN_BACK, INPUT_PULLUP);
    pinMode(BTN_FWD,  INPUT_PULLUP);

    ui_init();
    ui_update_net_status(net_get_state(), net_get_ssid(), net_get_ip(), net_get_rssi());
    ui_update_battery(power_battery_pct(), power_is_charging());
    ui_show_screen(SCREEN_SPLASH);

    // Render the first frame before turning on the backlight — no white flash.
    lv_timer_handler();
    bl_on();

    Serial.println("Dashboard ready, connecting WiFi...");
}

static net_state_t last_net_state = NET_STATE_INIT;
static uint32_t    last_activity_ms = 0;  // millis of last user interaction

// Returns true when standby/night-mode should be active right now.
// When NTP is unsynced, treats every hour as standby-allowed (conservative).
static bool standby_allowed_now(void) {
    const DevSettings* s = settings_get();
    if (!s->night_en) return true;
    time_t now = time(nullptr);
    if (now < 1000000L) return true;  // NTP not synced: allow standby
    time_t local = now + (int32_t)s->tz_offset * 3600;
    struct tm* t = gmtime(&local);
    int h = t->tm_hour;
    if (s->night_start <= s->night_end) {
        return (h >= s->night_start && h < s->night_end);
    } else {
        return (h >= s->night_start || h < s->night_end);
    }
}

void loop() {
#if BOARD_HAS_TOUCH
    poll_touch();

    // Wake from standby on touch (consume the touch so it doesn't also navigate)
    if (bl_is_standby() && touch_pressed) {
        bl_wake();
        last_activity_ms = millis();
        touch_pressed = false;
    }
#endif

    lv_timer_handler();
    ui_tick_anim();
    ui_tick_clock();
    net_tick();
    power_tick();
    splash_tick();

    // Track touch activity for standby timer
    if (touch_pressed) last_activity_ms = millis();

    {
        static bool back_was = false, fwd_was = false;
        static uint32_t back_down_ms = 0;
        // Start true: GPIO0 goes LOW during USB flash/boot-mode then releases,
        // which would look like a button press+release and navigate away from splash.
        static bool long_fired = true;
        bool back_now = (digitalRead(BTN_BACK) == LOW);
        bool fwd_now  = (digitalRead(BTN_FWD)  == LOW);

        if (back_now || fwd_now) last_activity_ms = millis();

        if (back_now != back_was) {
            if (back_now) {
                back_down_ms = millis();
                long_fired = false;
            } else if (!long_fired) {
                if (bl_is_standby()) {
                    // Button release wakes — don't also navigate
                } else if (ui_get_current_screen() == SCREEN_SPLASH) {
                    ui_show_screen(SCREEN_USAGE);
                } else {
                    ui_cycle_screen();
                }
            }
            back_was = back_now;
        }
        if (back_now && !long_fired && millis() - back_down_ms > BTN_LONG_PRESS_MS) {
            long_fired = true;
            if (bl_is_standby()) {
                bl_wake();
            } else {
                Serial.println("Long-press BOOT: clearing config, rebooting");
                cfg_clear();
                delay(200);
                ESP.restart();
            }
        }
        if (fwd_now != fwd_was) {
            if (fwd_now && !bl_is_standby() && net_get_state() == NET_STATE_CONNECTED) {
                Serial.println("Manual poll trigger");
                if (poll_force_now(&usage)) {
                    usage_rate_sample(usage.session_pct);
                    ui_update(&usage);
                }
            }
            fwd_was = fwd_now;
        }
    }

    // Auto-standby / auto-wake based on night-mode schedule.
    {
        const DevSettings* s = settings_get();
        if (bl_is_standby()) {
            // Auto-wake when the night window ends and night mode is configured.
            if (s->standby_en && s->night_en && !standby_allowed_now()) {
                bl_wake();
                last_activity_ms = millis();
            }
        } else if (s->standby_en) {
            uint32_t timeout_ms = (uint32_t)s->standby_min * 60000UL;
            if (millis() - last_activity_ms > timeout_ms && standby_allowed_now()) {
                bl_standby();
            }
        }
    }

    net_state_t ns = net_get_state();
    static const char* last_ssid = "";
    static const char* last_ip   = "";
    static int last_rssi = -999;
    static bool tz_synced = false;
    int rssi = net_get_rssi();
    if (ns != last_net_state || last_ssid != net_get_ssid() ||
        last_ip != net_get_ip() || last_rssi != rssi) {
        if (ns == NET_STATE_CONNECTED && !tz_synced) {
            tz_synced = true;
            tz_auto_sync();
        }
        last_net_state = ns;
        last_ssid = net_get_ssid();
        last_ip   = net_get_ip();
        last_rssi = rssi;
        ui_update_net_status(ns, last_ssid, last_ip, rssi);
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

    // Pairing state machine: when WiFi is up but tokens are missing/invalid,
    // run OAuth pairing on-device via QR. Skips polling entirely while active.
    {
        static bool pair_screen_shown = false;
        if (net_get_state() == NET_STATE_CONNECTED && !cfg_has_tokens()) {
            if (!pair_is_active()) {
                pair_start();
            }
            if (!pair_screen_shown) {
                ui_pair_set(pair_get_auth_url(), pair_get_lan_url(), pair_get_status_msg());
                ui_show_screen(SCREEN_PAIR);
                pair_screen_shown = true;
            } else {
                // Refresh status line in case it changed (exchange progress).
                ui_pair_set(nullptr, nullptr, pair_get_status_msg());
            }
            pair_tick();
            delay(5);
            return;
        }
        pair_screen_shown = false;
    }

    bool updated = false;
    poll_tick(&usage, &updated);
    if (updated) {
        int g_before = usage_rate_group();
        usage_rate_sample(usage.session_pct);
        int g_after = usage_rate_group();
        if (g_after != g_before) {
            Serial.printf("usage rate: group %d -> %d (s=%.2f%%)\n",
                          g_before, g_after, usage.session_pct);
            if (splash_is_active()) splash_pick_for_current_rate();
        }
        ui_update(&usage);
    }

    delay(5);
}
