#include "ui.h"
#include "splash.h"
#include "wifi_cfg.h"
#include "qr_render.h"
#include "backlight.h"
#include "settings_mgr.h"
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include "logo.h"
#include "icons.h"
#include "display_cfg.h"

LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_20);

#define FONT_TITLE   font_styrene_28
#define FONT_BIG     font_styrene_28
#define FONT_MEDIUM  font_styrene_20
#define FONT_SMALL   font_styrene_20
#define FONT_TINY    font_styrene_20
#define FONT_ANIM    font_styrene_20

// Anthropic brand palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// ---- Layout constants for 320x240 ----
#define SCR_W         320
#define SCR_H         240
#define MARGIN        10
#define TITLE_Y       6
#define CONTENT_Y     36
#define CONTENT_W     (SCR_W - 2 * MARGIN)

// ---- Usage screen widgets ----
static lv_obj_t* usage_container;
static lv_obj_t* lbl_title;
static lv_obj_t* bar_session;
static lv_obj_t* lbl_session_pct;
static lv_obj_t* lbl_session_label;
static lv_obj_t* lbl_session_reset;
static lv_obj_t* bar_weekly;
static lv_obj_t* lbl_weekly_pct;
static lv_obj_t* lbl_weekly_label;
static lv_obj_t* lbl_weekly_reset;
static lv_obj_t* lbl_anim;

// ---- Splash clock overlay ----
static lv_obj_t* clock_lbl;

// ---- Usage screen clock overlay ----
static lv_obj_t* clock_usage_lbl;

// ---- Settings: timezone label ----
static lv_obj_t* settings_tz_lbl;

// ---- Network screen widgets ----
static lv_obj_t* net_container;
static lv_obj_t* lbl_net_status;
static lv_obj_t* lbl_net_ssid;
static lv_obj_t* lbl_net_ip;
static lv_obj_t* lbl_net_rssi;

// ---- Settings screen widgets ----
static lv_obj_t* settings_container;
static lv_obj_t* settings_bl_slider;
static lv_obj_t* settings_bl_lbl;
static lv_obj_t* settings_standby_sw;
static lv_obj_t* settings_standby_lbl;
static lv_obj_t* settings_night_sw;
static lv_obj_t* settings_night_start_lbl;
static lv_obj_t* settings_night_end_lbl;

static const uint16_t standby_options[] = {5, 10, 15, 20, 30, 45, 60};
#define STANDBY_OPT_COUNT 7
static int settings_standby_opt_idx = 1;  // default: 10 min

// ---- Pair screen widgets ----
static lv_obj_t* pair_container;
static lv_obj_t* pair_qr_img;
static lv_obj_t* lbl_pair_lan;
static lv_obj_t* lbl_pair_status;
static String    pair_cur_auth;

// ---- Battery indicator (shared, on top) ----
static lv_obj_t* battery_img;
static lv_obj_t* logo_img;
static lv_image_dsc_t battery_dscs[5];  // empty, low, medium, full, charging

// ---- Shared ----
static lv_image_dsc_t logo_dsc;
static screen_t current_screen = SCREEN_USAGE;

// Animation state
static uint32_t anim_last_ms = 0;
static uint8_t anim_spinner_idx = 0;
static uint8_t anim_phase = 0;
static uint8_t anim_msg_idx = 0;
static uint32_t anim_msg_start = 0;
#define ANIM_MSG_MS     4000

static const char* const spinner_frames[] = {
    "\xC2\xB7", "\xE2\x9C\xBB", "\xE2\x9C\xBD",
    "\xE2\x9C\xB6", "\xE2\x9C\xB3", "\xE2\x9C\xA2",
};
#define SPINNER_COUNT 6
#define SPINNER_PHASES (2 * (SPINNER_COUNT - 1))  // 10: ping-pong 0..5..0

// Per-frame hold time. Modeled on Claude Code's spinner (Cavalry triangle
// oscillator, range 0..5, period 5s) — turn-around frames (0 and 5) appear
// once per cycle, middle frames twice, so 0/5 read as held longer.
static const uint16_t spinner_ms[SPINNER_COUNT] = {
    260, 130, 130, 130, 130, 260,
};

static const char* const anim_messages[] = {
    "Accomplishing", "Elucidating", "Perusing",
    "Actioning", "Enchanting", "Philosophising",
    "Actualizing", "Envisioning", "Pondering",
    "Baking", "Finagling", "Pontificating",
    "Booping", "Flibbertigibbeting", "Processing",
    "Brewing", "Forging", "Puttering",
    "Calculating", "Forming", "Puzzling",
    "Cerebrating", "Frolicking", "Reticulating",
    "Channelling", "Generating", "Ruminating",
    "Churning", "Germinating", "Scheming",
    "Clauding", "Hatching", "Schlepping",
    "Coalescing", "Herding", "Shimmying",
    "Cogitating", "Honking", "Shucking",
    "Combobulating", "Hustling", "Simmering",
    "Computing", "Ideating", "Smooshing",
    "Concocting", "Imagining", "Spelunking",
    "Conjuring", "Incubating", "Spinning",
    "Considering", "Inferring", "Stewing",
    "Contemplating", "Jiving", "Sussing",
    "Cooking", "Manifesting", "Synthesizing",
    "Crafting", "Marinating", "Thinking",
    "Creating", "Meandering", "Tinkering",
    "Crunching", "Moseying", "Transmuting",
    "Deciphering", "Mulling", "Unfurling",
    "Deliberating", "Mustering", "Unravelling",
    "Determining", "Musing", "Vibing",
    "Discombobulating", "Noodling", "Wandering",
    "Divining", "Percolating", "Whirring",
    "Doing", "Wibbling",
    "Effecting", "Wizarding",
    "Working", "Wrangling",
};
#define ANIM_MSG_COUNT (sizeof(anim_messages) / sizeof(anim_messages[0]))

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

static void format_reset_time(int mins, char* buf, size_t len) {
    if (mins < 0) {
        snprintf(buf, len, "---");
    } else if (mins < 60) {
        snprintf(buf, len, "Resets in %dm", mins);
    } else if (mins < 1440) {
        snprintf(buf, len, "Resets in %dh %dm", mins / 60, mins % 60);
    } else {
        snprintf(buf, len, "Resets in %dd %dh", mins / 1440, (mins % 1440) / 60);
    }
}

// Forward decls — callbacks defined near ui_show_screen below
static void global_click_cb(lv_event_t* e);
static void net_reset_click_cb(lv_event_t* e);
static void bl_slider_cb(lv_event_t* e);
static void standby_sw_cb(lv_event_t* e);
static void standby_btn_cb(lv_event_t* e);
static void night_sw_cb(lv_event_t* e);
static void night_start_btn_cb(lv_event_t* e);
static void night_end_btn_cb(lv_event_t* e);
static void tz_btn_cb(lv_event_t* e);

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_left(panel, 10, 0);
    lv_obj_set_style_pad_right(panel, 10, 0);
    lv_obj_set_style_pad_top(panel, 5, 0);
    lv_obj_set_style_pad_bottom(panel, 5, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    // Bubble click events up to the screen / usage_container so a tap anywhere
    // on the panel fires the global click handler.
    lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    return panel;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    return bar;
}

static void init_icon_dsc(lv_image_dsc_t* dsc, int w, int h, const uint16_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.stride = w * 2;
    dsc->data = (const uint8_t*)data;
    dsc->data_size = w * h * 2;
}

// RGB565A8: planar — w*h RGB565 pixels followed by w*h alpha bytes.
// Stride is RGB565-only (w*2); LVGL infers alpha plane location from header.
static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

static lv_obj_t* make_pill(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_bg_color(lbl, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lbl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(lbl, 18, 0);
    lv_obj_set_style_pad_right(lbl, 18, 0);
    lv_obj_set_style_pad_top(lbl, 6, 0);
    lv_obj_set_style_pad_bottom(lbl, 6, 0);
    return lbl;
}

// ---- Battery icon initialization ----
static void init_battery_icons(void) {
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);
}

// ======== Usage Screen (480x480) ========

#define PANEL_H     78
#define PANEL_GAP   6

// One Session/Weekly panel: big % label, pill on the right, bar, reset label.
// Pill y=1: symmetric inside the panel — panel-outer-top → pill-top equals
// pill-bottom → bar-top (pill height 42 + panel pad_top 12 + bar y=56).
static void make_usage_panel(lv_obj_t* parent, int y, const char* pill_text,
                             lv_obj_t** out_pct, lv_obj_t** out_pill,
                             lv_obj_t** out_bar, lv_obj_t** out_reset) {
    lv_obj_t* panel = make_panel(parent, MARGIN, y, CONTENT_W, PANEL_H);

    *out_pct = lv_label_create(panel);
    lv_label_set_text(*out_pct, "---%");
    lv_obj_set_style_text_font(*out_pct, &FONT_BIG, 0);
    lv_obj_set_style_text_color(*out_pct, COL_TEXT, 0);
    lv_obj_set_pos(*out_pct, 0, 0);

    *out_pill = make_pill(panel, pill_text);
    lv_obj_align(*out_pill, LV_ALIGN_TOP_RIGHT, 0, -3);

    *out_bar = make_bar(panel, 0, 32, CONTENT_W - 32, 14);
    *out_reset = lv_label_create(panel);
    lv_label_set_text(*out_reset, "---");
    lv_obj_set_style_text_font(*out_reset, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(*out_reset, COL_DIM, 0);
    lv_obj_set_pos(*out_reset, 0, 52);
}

static void init_usage_screen(lv_obj_t* scr) {
    usage_container = lv_obj_create(scr);
    lv_obj_set_size(usage_container, SCR_W, SCR_H);
    lv_obj_set_pos(usage_container, 0, 0);
    lv_obj_set_style_bg_opa(usage_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(usage_container, 0, 0);
    lv_obj_set_style_pad_all(usage_container, 0, 0);
    lv_obj_clear_flag(usage_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(usage_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    lbl_title = lv_label_create(usage_container);
    lv_label_set_text(lbl_title, "Usage");
    lv_obj_set_style_text_font(lbl_title, &FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 16, TITLE_Y);

    make_usage_panel(usage_container, CONTENT_Y, "Current",
                     &lbl_session_pct, &lbl_session_label,
                     &bar_session, &lbl_session_reset);
    make_usage_panel(usage_container, CONTENT_Y + PANEL_H + PANEL_GAP, "Weekly",
                     &lbl_weekly_pct, &lbl_weekly_label,
                     &bar_weekly, &lbl_weekly_reset);

    lbl_anim = lv_label_create(usage_container);
    lv_label_set_text(lbl_anim, "");
    lv_obj_set_style_text_font(lbl_anim, &FONT_ANIM, 0);
    lv_obj_set_style_text_color(lbl_anim, COL_ACCENT, 0);
    lv_obj_set_width(lbl_anim, 200);
    lv_label_set_long_mode(lbl_anim, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl_anim, LV_ALIGN_BOTTOM_LEFT, MARGIN, -4);

    clock_usage_lbl = lv_label_create(usage_container);
    lv_label_set_text(clock_usage_lbl, "--:--");
    lv_obj_set_style_text_font(clock_usage_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(clock_usage_lbl, COL_DIM, 0);
    lv_obj_set_style_bg_color(clock_usage_lbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(clock_usage_lbl, LV_OPA_50, 0);
    lv_obj_set_style_radius(clock_usage_lbl, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(clock_usage_lbl, 8, 0);
    lv_obj_set_style_pad_right(clock_usage_lbl, 8, 0);
    lv_obj_set_style_pad_top(clock_usage_lbl, 3, 0);
    lv_obj_set_style_pad_bottom(clock_usage_lbl, 3, 0);
    lv_obj_align(clock_usage_lbl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_add_flag(clock_usage_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
}

// ======== Network Screen ========

static void init_network_screen(lv_obj_t* scr) {
    net_container = lv_obj_create(scr);
    lv_obj_set_size(net_container, SCR_W, SCR_H);
    lv_obj_set_pos(net_container, 0, 0);
    lv_obj_set_style_bg_opa(net_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(net_container, 0, 0);
    lv_obj_set_style_pad_all(net_container, 0, 0);
    lv_obj_clear_flag(net_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_net_title = lv_label_create(net_container);
    lv_label_set_text(lbl_net_title, "Network");
    lv_obj_set_style_text_font(lbl_net_title, &FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_net_title, COL_TEXT, 0);
    lv_obj_align(lbl_net_title, LV_ALIGN_TOP_MID, 16, TITLE_Y);

    // 4 rows: status / ssid / ip / signal — each 24px, with 4px gaps
    const int INFO_H  = 116;
    const int RESET_H = 60;
    lv_obj_t* p_info = make_panel(net_container, MARGIN, CONTENT_Y, CONTENT_W, INFO_H);

    lbl_net_status = lv_label_create(p_info);
    lv_label_set_text(lbl_net_status, "Initializing...");
    lv_obj_set_style_text_font(lbl_net_status, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_net_status, COL_DIM, 0);
    lv_obj_set_pos(lbl_net_status, 0, 0);

    lbl_net_ssid = lv_label_create(p_info);
    lv_label_set_text(lbl_net_ssid, "SSID: ---");
    lv_obj_set_style_text_font(lbl_net_ssid, &FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_net_ssid, COL_DIM, 0);
    lv_obj_set_pos(lbl_net_ssid, 0, 26);

    lbl_net_ip = lv_label_create(p_info);
    lv_label_set_text(lbl_net_ip, "IP: ---");
    lv_obj_set_style_text_font(lbl_net_ip, &FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_net_ip, COL_DIM, 0);
    lv_obj_set_pos(lbl_net_ip, 0, 52);

    lbl_net_rssi = lv_label_create(p_info);
    lv_label_set_text(lbl_net_rssi, "");
    lv_obj_set_style_text_font(lbl_net_rssi, &FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_net_rssi, COL_DIM, 0);
    lv_obj_set_pos(lbl_net_rssi, 0, 78);

    int reset_y = CONTENT_Y + INFO_H + 8;
    lv_obj_t* reset_zone = lv_obj_create(net_container);
    lv_obj_set_pos(reset_zone, MARGIN, reset_y);
    lv_obj_set_size(reset_zone, CONTENT_W, RESET_H);
    lv_obj_set_style_bg_color(reset_zone, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(reset_zone, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(reset_zone, 8, 0);
    lv_obj_set_style_border_width(reset_zone, 0, 0);
    lv_obj_set_style_pad_column(reset_zone, 14, 0);
    lv_obj_set_flex_flow(reset_zone, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_zone, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(reset_zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(reset_zone, net_reset_click_cb, LV_EVENT_CLICKED, NULL);

    static lv_image_dsc_t icon_trash_dsc;
    init_icon_dsc(&icon_trash_dsc, ICON_TRASH2_W, ICON_TRASH2_H, icon_trash2_data);
    lv_obj_t* trash_img = lv_image_create(reset_zone);
    lv_image_set_src(trash_img, &icon_trash_dsc);
    lv_image_set_scale(trash_img, 160);

    lv_obj_t* reset_lbl = lv_label_create(reset_zone);
    lv_label_set_text(reset_lbl, "Reset config");
    lv_obj_set_style_text_font(reset_lbl, &FONT_SMALL, 0);
    lv_obj_set_style_text_color(reset_lbl, COL_DIM, 0);

    lv_obj_add_flag(net_container, LV_OBJ_FLAG_HIDDEN);
}

// ======== Settings Screen ========

static lv_obj_t* make_small_btn(lv_obj_t* parent, const char* text,
                                 lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 30, 28);
    lv_obj_set_style_bg_color(btn, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_center(lbl);
    return btn;
}

static void init_settings_screen(lv_obj_t* scr) {
    const DevSettings* s = settings_get();

    settings_standby_opt_idx = 1;  // fallback: 10 min
    for (int i = 0; i < STANDBY_OPT_COUNT; i++) {
        if (standby_options[i] == s->standby_min) {
            settings_standby_opt_idx = i;
            break;
        }
    }

    settings_container = lv_obj_create(scr);
    lv_obj_set_size(settings_container, SCR_W, SCR_H);
    lv_obj_set_pos(settings_container, 0, 0);
    lv_obj_set_style_bg_opa(settings_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(settings_container, 0, 0);
    lv_obj_set_style_pad_all(settings_container, 0, 0);
    lv_obj_set_scroll_dir(settings_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_container, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* lbl_title = lv_label_create(settings_container);
    lv_label_set_text(lbl_title, "Settings");
    lv_obj_set_style_text_font(lbl_title, &FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 16, TITLE_Y);

    // ---- Panel 1: Brightness ----
    // h=50 (inner=40): label at y=0 (h≈20), slider bar at y=28 → knob top ≈y=22 → 2px gap
    lv_obj_t* p_bl = make_panel(settings_container, MARGIN, CONTENT_Y, CONTENT_W, 50);
    lv_obj_clear_flag(p_bl, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE));

    lv_obj_t* lbl_bl_title = lv_label_create(p_bl);
    lv_label_set_text(lbl_bl_title, "Brightness");
    lv_obj_set_style_text_font(lbl_bl_title, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_bl_title, COL_TEXT, 0);
    lv_obj_set_pos(lbl_bl_title, 0, 0);

    settings_bl_lbl = lv_label_create(p_bl);
    char pct_buf[8];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", s->brightness * 100 / 255);
    lv_label_set_text(settings_bl_lbl, pct_buf);
    lv_obj_set_style_text_font(settings_bl_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(settings_bl_lbl, COL_ACCENT, 0);
    lv_obj_align(settings_bl_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

    settings_bl_slider = lv_slider_create(p_bl);
    lv_obj_set_size(settings_bl_slider, CONTENT_W - 20, 12);
    lv_obj_set_pos(settings_bl_slider, 0, 28);  // explicit y: label(h≈20) + 8px gap
    lv_slider_set_range(settings_bl_slider, 5, 100);
    lv_slider_set_value(settings_bl_slider, s->brightness * 100 / 255, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(settings_bl_slider, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_bl_slider, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(settings_bl_slider, COL_TEXT, LV_PART_KNOB);
    lv_obj_add_event_cb(settings_bl_slider, bl_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ---- Panel 2: Auto-standby ----
    lv_obj_t* p_st = make_panel(settings_container, MARGIN, CONTENT_Y + 50 + 2, CONTENT_W, 76);
    lv_obj_clear_flag(p_st, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE));

    lv_obj_t* lbl_st = lv_label_create(p_st);
    lv_label_set_text(lbl_st, "Auto-standby");
    lv_obj_set_style_text_font(lbl_st, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_st, COL_TEXT, 0);
    lv_obj_set_pos(lbl_st, 0, 4);

    settings_standby_sw = lv_switch_create(p_st);
    lv_obj_set_size(settings_standby_sw, 48, 24);
    lv_obj_align(settings_standby_sw, LV_ALIGN_TOP_RIGHT, 0, 0);
    if (s->standby_en) lv_obj_add_state(settings_standby_sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(settings_standby_sw, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_standby_sw, COL_ACCENT,
                              (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
    lv_obj_add_event_cb(settings_standby_sw, standby_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* lbl_after = lv_label_create(p_st);
    lv_label_set_text(lbl_after, "After");
    lv_obj_set_style_text_font(lbl_after, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_after, COL_DIM, 0);
    lv_obj_set_pos(lbl_after, 0, 38);

    make_small_btn(p_st, "<", standby_btn_cb, (void*)(intptr_t)-1);
    lv_obj_set_pos(lv_obj_get_child(p_st, lv_obj_get_child_count(p_st) - 1), 60, 34);

    settings_standby_lbl = lv_label_create(p_st);
    char min_buf[12];
    snprintf(min_buf, sizeof(min_buf), "%d min", standby_options[settings_standby_opt_idx]);
    lv_label_set_text(settings_standby_lbl, min_buf);
    lv_obj_set_style_text_font(settings_standby_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(settings_standby_lbl, COL_ACCENT, 0);
    lv_obj_set_pos(settings_standby_lbl, 95, 38);

    make_small_btn(p_st, ">", standby_btn_cb, (void*)(intptr_t)+1);
    lv_obj_set_pos(lv_obj_get_child(p_st, lv_obj_get_child_count(p_st) - 1), 170, 34);

    // ---- Panel 3: Night hours ----
    lv_obj_t* p_nt = make_panel(settings_container, MARGIN,
                                 CONTENT_Y + 50 + 2 + 76 + 2, CONTENT_W, 72);
    lv_obj_clear_flag(p_nt, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE));

    lv_obj_t* lbl_nt = lv_label_create(p_nt);
    lv_label_set_text(lbl_nt, "Night only");
    lv_obj_set_style_text_font(lbl_nt, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_nt, COL_TEXT, 0);
    lv_obj_set_pos(lbl_nt, 0, 4);

    settings_night_sw = lv_switch_create(p_nt);
    lv_obj_set_size(settings_night_sw, 48, 24);
    lv_obj_align(settings_night_sw, LV_ALIGN_TOP_RIGHT, 0, 0);
    if (s->night_en) lv_obj_add_state(settings_night_sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(settings_night_sw, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(settings_night_sw, COL_ACCENT,
                              (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
    lv_obj_add_event_cb(settings_night_sw, night_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Row 2: [<] HHh [>]  -  [<] HHh [>]
    // "22h" = 3 chars, ~36px in font_20; btn=30px; gap=4px → total 30+4+36+4+30=104 per side
    // Layout (inner width=280): 0,34,68,110,140,174,208,250
    make_small_btn(p_nt, "<", night_start_btn_cb, (void*)(intptr_t)-1);
    lv_obj_set_pos(lv_obj_get_child(p_nt, lv_obj_get_child_count(p_nt) - 1), 0, 36);

    settings_night_start_lbl = lv_label_create(p_nt);
    char h_buf[6];
    snprintf(h_buf, sizeof(h_buf), "%dh", s->night_start);
    lv_label_set_text(settings_night_start_lbl, h_buf);
    lv_obj_set_style_text_font(settings_night_start_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(settings_night_start_lbl, COL_ACCENT, 0);
    lv_obj_set_pos(settings_night_start_lbl, 34, 40);

    make_small_btn(p_nt, ">", night_start_btn_cb, (void*)(intptr_t)+1);
    lv_obj_set_pos(lv_obj_get_child(p_nt, lv_obj_get_child_count(p_nt) - 1), 76, 36);

    lv_obj_t* lbl_sep = lv_label_create(p_nt);
    lv_label_set_text(lbl_sep, "-");
    lv_obj_set_style_text_font(lbl_sep, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_sep, COL_DIM, 0);
    lv_obj_set_pos(lbl_sep, 114, 40);

    make_small_btn(p_nt, "<", night_end_btn_cb, (void*)(intptr_t)-1);
    lv_obj_set_pos(lv_obj_get_child(p_nt, lv_obj_get_child_count(p_nt) - 1), 132, 36);

    settings_night_end_lbl = lv_label_create(p_nt);
    snprintf(h_buf, sizeof(h_buf), "%dh", s->night_end);
    lv_label_set_text(settings_night_end_lbl, h_buf);
    lv_obj_set_style_text_font(settings_night_end_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(settings_night_end_lbl, COL_ACCENT, 0);
    lv_obj_set_pos(settings_night_end_lbl, 166, 40);

    make_small_btn(p_nt, ">", night_end_btn_cb, (void*)(intptr_t)+1);
    lv_obj_set_pos(lv_obj_get_child(p_nt, lv_obj_get_child_count(p_nt) - 1), 208, 36);

    // ---- Panel 4: Timezone offset (scrolled into view) ----
    int p4_y = CONTENT_Y + 50 + 2 + 76 + 2 + 72 + 2;
    lv_obj_t* p_tz = make_panel(settings_container, MARGIN, p4_y, CONTENT_W, 48);
    lv_obj_clear_flag(p_tz, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE));

    lv_obj_t* lbl_tz_title = lv_label_create(p_tz);
    lv_label_set_text(lbl_tz_title, "Timezone");
    lv_obj_set_style_text_font(lbl_tz_title, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_tz_title, COL_TEXT, 0);
    lv_obj_set_pos(lbl_tz_title, 0, 4);

    make_small_btn(p_tz, "<", tz_btn_cb, (void*)(intptr_t)-1);
    lv_obj_set_pos(lv_obj_get_child(p_tz, lv_obj_get_child_count(p_tz) - 1), 80, 14);

    settings_tz_lbl = lv_label_create(p_tz);
    {
        char tz_buf[10];
        int8_t tz = s->tz_offset;
        if (tz == 0) snprintf(tz_buf, sizeof(tz_buf), "UTC");
        else         snprintf(tz_buf, sizeof(tz_buf), "UTC%+d", (int)tz);
        lv_label_set_text(settings_tz_lbl, tz_buf);
    }
    lv_obj_set_style_text_font(settings_tz_lbl, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(settings_tz_lbl, COL_ACCENT, 0);
    lv_obj_set_pos(settings_tz_lbl, 115, 18);

    make_small_btn(p_tz, ">", tz_btn_cb, (void*)(intptr_t)+1);
    lv_obj_set_pos(lv_obj_get_child(p_tz, lv_obj_get_child_count(p_tz) - 1), 174, 14);

    lv_obj_add_flag(settings_container, LV_OBJ_FLAG_HIDDEN);
}

// ======== Pair Screen ========

static void init_pair_screen(lv_obj_t* scr) {
    pair_container = lv_obj_create(scr);
    lv_obj_set_size(pair_container, SCR_W, SCR_H);
    lv_obj_set_pos(pair_container, 0, 0);
    lv_obj_set_style_bg_color(pair_container, COL_BG, 0);
    lv_obj_set_style_bg_opa(pair_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pair_container, 0, 0);
    lv_obj_set_style_pad_all(pair_container, 0, 0);
    lv_obj_clear_flag(pair_container, LV_OBJ_FLAG_SCROLLABLE);

    // Vertical stack, centered. QR top, full-width URL + status beneath it.
    lv_obj_t* lbl_title = lv_label_create(pair_container);
    lv_label_set_text(lbl_title, "Pair Clawdmeter");
    lv_obj_set_style_text_font(lbl_title, &FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 2);

    // QR image, horizontally centered. Size is set by ui_pair_set() via
    // lv_image_set_src(); recenter every refresh via the image's own align.
    pair_qr_img = lv_image_create(pair_container);
    lv_obj_align(pair_qr_img, LV_ALIGN_TOP_MID, 0, 28);

    lbl_pair_lan = lv_label_create(pair_container);
    lv_label_set_text(lbl_pair_lan, "...");
    lv_obj_set_style_text_font(lbl_pair_lan, &FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_pair_lan, COL_TEXT, 0);
    lv_obj_set_style_text_align(lbl_pair_lan, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_pair_lan, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_pair_lan, SCR_W - 8);
    lv_obj_align(lbl_pair_lan, LV_ALIGN_BOTTOM_MID, 0, -28);

    lbl_pair_status = lv_label_create(pair_container);
    lv_label_set_text(lbl_pair_status, "");
    lv_obj_set_style_text_font(lbl_pair_status, &FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_pair_status, COL_AMBER, 0);
    lv_obj_set_style_text_align(lbl_pair_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl_pair_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_pair_status, SCR_W - 8);
    lv_obj_align(lbl_pair_status, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_obj_add_flag(pair_container, LV_OBJ_FLAG_HIDDEN);
}

// ======== Public API ========

void ui_init(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Logo (shared, always visible, on top of all containers)
    // Logo is RGB565A8 (planar: w*h RGB565 then w*h alpha) so it composites
    // cleanly against whatever bg is behind it.
    init_icon_dsc_rgb565a8(&logo_dsc, LOGO_WIDTH, LOGO_HEIGHT, logo_data);

    // Initialize battery icon descriptors
    init_battery_icons();

    init_usage_screen(scr);
    init_network_screen(scr);
    init_settings_screen(scr);
    init_pair_screen(scr);
    splash_init(scr);

    // Splash is touch-toggled — tap anywhere on the splash dismisses it
    if (splash_get_root()) {
        lv_obj_add_event_cb(splash_get_root(), global_click_cb, LV_EVENT_CLICKED, NULL);
    }

    // Clock overlay on splash — bottom-center, above the animation canvas
    if (splash_get_root()) {
        clock_lbl = lv_label_create(splash_get_root());
        lv_label_set_text(clock_lbl, "--:--");
        lv_obj_set_style_text_font(clock_lbl, &FONT_MEDIUM, 0);
        lv_obj_set_style_text_color(clock_lbl, COL_DIM, 0);
        lv_obj_set_style_bg_color(clock_lbl, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(clock_lbl, LV_OPA_50, 0);
        lv_obj_set_style_radius(clock_lbl, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_left(clock_lbl, 8, 0);
        lv_obj_set_style_pad_right(clock_lbl, 8, 0);
        lv_obj_set_style_pad_top(clock_lbl, 3, 0);
        lv_obj_set_style_pad_bottom(clock_lbl, 3, 0);
        lv_obj_align(clock_lbl, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
        lv_obj_add_flag(clock_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    // Logo on top of all containers (inset for rounded corners)
    logo_img = lv_image_create(scr);
    lv_image_set_src(logo_img, &logo_dsc);
    lv_obj_set_pos(logo_img, MARGIN, TITLE_Y - 10);

    // Battery indicator on top of all containers (upper-right, inset)
    battery_img = lv_image_create(scr);
    lv_image_set_src(battery_img, &battery_dscs[0]);
    lv_obj_set_pos(battery_img, SCR_W - 48 - MARGIN, TITLE_Y);

    // The 80x80 brand logo and battery indicator don't fit (and aren't
    // applicable) on the 320x240 BOX. Hide both — the title label alone
    // identifies the screen.
    lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
}

void ui_update(const UsageData* data) {
    if (!data->valid) return;

    int s_pct = (int)(data->session_pct + 0.5f);

    // Usage screen
    lv_label_set_text_fmt(lbl_session_pct, "%d%%", s_pct);
    lv_bar_set_value(bar_session, s_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_session, pct_color(data->session_pct), LV_PART_INDICATOR);

    char buf[48];
    format_reset_time(data->session_reset_mins, buf, sizeof(buf));
    lv_label_set_text(lbl_session_reset, buf);

    int w_pct = (int)(data->weekly_pct + 0.5f);
    lv_label_set_text_fmt(lbl_weekly_pct, "%d%%", w_pct);
    lv_bar_set_value(bar_weekly, w_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_weekly, pct_color(data->weekly_pct), LV_PART_INDICATOR);

    format_reset_time(data->weekly_reset_mins, buf, sizeof(buf));
    lv_label_set_text(lbl_weekly_reset, buf);
}

void ui_tick_anim(void) {
    if (current_screen != SCREEN_USAGE) return;

    uint32_t now = lv_tick_get();

    if (now - anim_msg_start >= ANIM_MSG_MS) {
        anim_msg_idx = (anim_msg_idx + 1) % ANIM_MSG_COUNT;
        anim_msg_start = now;
    }

    if (now - anim_last_ms >= spinner_ms[anim_spinner_idx]) {
        anim_last_ms = now;
        anim_phase = (anim_phase + 1) % SPINNER_PHASES;
        anim_spinner_idx = (anim_phase < SPINNER_COUNT) ? anim_phase
                                                        : (SPINNER_PHASES - anim_phase);

        static char buf[80];
        snprintf(buf, sizeof(buf), "%s %s\xE2\x80\xA6",
                 spinner_frames[anim_spinner_idx],
                 anim_messages[anim_msg_idx]);
        lv_label_set_text(lbl_anim, buf);
    }
}

static screen_t prev_non_splash_screen = SCREEN_USAGE;
// Hide the battery indicator on the splash screen — the icon is visually
// noisy over the pixel-art creature animations.
static void apply_battery_visibility(void) {
    if (!battery_img) return;
    // No battery on the BOX — icon stays hidden in every screen.
    lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);
}

// LVGL handles click debouncing internally. Screen-level handler fires when
// no child consumed the event (children only consume if they have their own
// event callback, e.g. the Reset Bluetooth zone). On BT screen we skip the
// splash toggle so only the reset zone is interactive there.
static void global_click_cb(lv_event_t* e) {
    (void)e;
    screen_t s = ui_get_current_screen();
    if (s == SCREEN_NETWORK || s == SCREEN_SETTINGS || s == SCREEN_PAIR) return;
    ui_toggle_splash();
}

static void net_reset_click_cb(lv_event_t* e) {
    (void)e;
    cfg_clear();
    delay(200);
    ESP.restart();
}

static void bl_slider_cb(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int pct = lv_slider_get_value(slider);
    uint8_t val = (uint8_t)(pct * 255 / 100);
    bl_set(val);
    settings_set_brightness(val);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(settings_bl_lbl, buf);
}

static void standby_sw_cb(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    settings_set_standby(en, settings_get()->standby_min);
}

static void standby_btn_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    settings_standby_opt_idx =
        (settings_standby_opt_idx + dir + STANDBY_OPT_COUNT) % STANDBY_OPT_COUNT;
    uint16_t min = standby_options[settings_standby_opt_idx];
    settings_set_standby(settings_get()->standby_en, min);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d min", min);
    lv_label_set_text(settings_standby_lbl, buf);
}

static void night_sw_cb(lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool en = lv_obj_has_state(sw, LV_STATE_CHECKED);
    const DevSettings* s = settings_get();
    settings_set_night(en, s->night_start, s->night_end);
}

static void night_start_btn_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const DevSettings* s = settings_get();
    uint8_t h = (uint8_t)((s->night_start + 24 + dir) % 24);
    settings_set_night(s->night_en, h, s->night_end);
    char buf[6];
    snprintf(buf, sizeof(buf), "%dh", h);
    lv_label_set_text(settings_night_start_lbl, buf);
}

static void night_end_btn_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const DevSettings* s = settings_get();
    uint8_t h = (uint8_t)((s->night_end + 24 + dir) % 24);
    settings_set_night(s->night_en, s->night_start, h);
    char buf[6];
    snprintf(buf, sizeof(buf), "%dh", h);
    lv_label_set_text(settings_night_end_lbl, buf);
}

static void tz_btn_cb(lv_event_t* e) {
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    int8_t tz = (int8_t)(settings_get()->tz_offset + dir);
    if (tz < -12) tz = -12;
    if (tz >  14) tz =  14;
    settings_set_tz_offset(tz);
    char buf[10];
    if (tz == 0) snprintf(buf, sizeof(buf), "UTC");
    else         snprintf(buf, sizeof(buf), "UTC%+d", (int)tz);
    lv_label_set_text(settings_tz_lbl, buf);
}

void ui_show_screen(screen_t screen) {
    lv_obj_add_flag(usage_container,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(net_container,      LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_container,     LV_OBJ_FLAG_HIDDEN);
    splash_hide();

    switch (screen) {
    case SCREEN_SPLASH:    splash_show(); break;
    case SCREEN_USAGE:     lv_obj_clear_flag(usage_container,    LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_NETWORK:   lv_obj_clear_flag(net_container,      LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_SETTINGS:  lv_obj_clear_flag(settings_container, LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_PAIR:      lv_obj_clear_flag(pair_container,     LV_OBJ_FLAG_HIDDEN); break;
    default: break;
    }

    // Hide the logo overlay on the splash screen so the animation has a clean canvas
    if (logo_img) {
        lv_obj_add_flag(logo_img, LV_OBJ_FLAG_HIDDEN);
    }

    if (screen != SCREEN_SPLASH) prev_non_splash_screen = screen;
    current_screen = screen;
    apply_battery_visibility();
}

void ui_cycle_screen(void) {
    if (current_screen == SCREEN_PAIR) return;
    screen_t next;
    switch (current_screen) {
    case SCREEN_USAGE:    next = SCREEN_NETWORK;   break;
    case SCREEN_NETWORK:  next = SCREEN_SETTINGS;  break;
    default:              next = SCREEN_USAGE;     break;
    }
    ui_show_screen(next);
}

void ui_toggle_splash(void) {
    if (current_screen == SCREEN_SPLASH) ui_show_screen(prev_non_splash_screen);
    else                                  ui_show_screen(SCREEN_SPLASH);
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_net_status(net_state_t state, const char* ssid, const char* ip, int rssi) {
    switch (state) {
    case NET_STATE_CONNECTED:
        lv_label_set_text(lbl_net_status, "Connected");
        lv_obj_set_style_text_color(lbl_net_status, COL_GREEN, 0);
        break;
    case NET_STATE_CONNECTING:
        lv_label_set_text(lbl_net_status, "Connecting...");
        lv_obj_set_style_text_color(lbl_net_status, COL_AMBER, 0);
        break;
    case NET_STATE_PORTAL:
        lv_label_set_text(lbl_net_status, "Setup mode");
        lv_obj_set_style_text_color(lbl_net_status, COL_ACCENT, 0);
        break;
    case NET_STATE_PAIRING:
        lv_label_set_text(lbl_net_status, "Pairing");
        lv_obj_set_style_text_color(lbl_net_status, COL_ACCENT, 0);
        break;
    case NET_STATE_FAILED:
        lv_label_set_text(lbl_net_status, "Failed");
        lv_obj_set_style_text_color(lbl_net_status, COL_RED, 0);
        break;
    default:
        lv_label_set_text(lbl_net_status, "Initializing...");
        lv_obj_set_style_text_color(lbl_net_status, COL_DIM, 0);
        break;
    }

    if (ssid) {
        static char sbuf[64];
        snprintf(sbuf, sizeof(sbuf), "SSID: %s", ssid);
        lv_label_set_text(lbl_net_ssid, sbuf);
    }
    if (ip) {
        static char ibuf[48];
        snprintf(ibuf, sizeof(ibuf), "IP: %s", ip);
        lv_label_set_text(lbl_net_ip, ibuf);
    }
    {
        static char rbuf[32];
        if (state == NET_STATE_CONNECTED && rssi != 0)
            snprintf(rbuf, sizeof(rbuf), "Signal: %d dBm", rssi);
        else
            rbuf[0] = '\0';
        lv_label_set_text(lbl_net_rssi, rbuf);
    }
}

void ui_pair_set(const char* auth_url, const char* lan_url, const char* status) {
    if (auth_url && pair_cur_auth != auth_url) {
        pair_cur_auth = auth_url;
        // QR fits between the title (~y=28) and the URL line (~y=190).
        // 160-px target leaves room above and below for readable text.
        if (qr_render(auth_url, 160) && pair_qr_img) {
            lv_image_set_src(pair_qr_img, qr_get_image());
            lv_obj_align(pair_qr_img, LV_ALIGN_TOP_MID, 0, 28);
        }
    }
    if (lan_url && lbl_pair_lan) {
        lv_label_set_text(lbl_pair_lan, lan_url);
    }
    if (status && lbl_pair_status) {
        lv_label_set_text(lbl_pair_status, status);
    }
}

void ui_update_battery(int percent, bool charging) {
    int idx;
    if (charging) {
        idx = 4;  // charging icon
    } else if (percent < 0) {
        idx = 0;  // no battery / unknown
    } else if (percent <= 10) {
        idx = 0;  // empty
    } else if (percent <= 35) {
        idx = 1;  // low
    } else if (percent <= 75) {
        idx = 2;  // medium
    } else {
        idx = 3;  // full
    }
    lv_image_set_src(battery_img, &battery_dscs[idx]);
    apply_battery_visibility();
}

void ui_tick_clock(void) {
    time_t raw = time(nullptr);
    if (raw < 1000000L) return;
    int8_t tz = settings_get()->tz_offset;
    time_t local = raw + (int32_t)tz * 3600;
    struct tm* t = gmtime(&local);
    static int last_min = -1;
    if (t->tm_min == last_min) return;
    last_min = t->tm_min;
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    if (clock_lbl)       lv_label_set_text(clock_lbl, buf);
    if (clock_usage_lbl) lv_label_set_text(clock_usage_lbl, buf);
}
