# Project context

## Commit rules

- **Never** add `Co-Authored-By: Claude` or any Claude/Anthropic attribution to commit messages.

## Writing style rules

- **Never** use the em dash character `-` in any written output (README, docs, comments, commit messages). Use a plain hyphen `-` instead. Em dashes are a known AI writing marker.

ESP32-S3 firmware for a desk-side Claude Code usage monitor on the **Espressif ESP32-S3-BOX** (320×240 IPS, ILI9342C, TT21100 touch). Device connects to WiFi and polls the Anthropic API directly for rate-limit utilization. No host daemon.

This file is for future Claude Code sessions to bootstrap quickly. Read this first.

## Hardware (critical pins - S3-BOX)

- Display: **ILI9342C** IPS 320×240 via SPI (defined in `display_cfg.h`)
- Touch: **TT21100** via I2C
- Buttons:
  - GPIO 0 (BOOT) - cycle screen / advance splash
  - GPIO 1 (mute slider) - refresh trigger (formerly Shift+Tab BLE HID, dropped)

## Architecture

```text
main.cpp        - setup(), loop(), button polling, screen cycle
display_cfg.h   - pin defines
ui.{h,cpp}      - 3-screen UI (splash, usage, network); network screen shows WiFi state + IP
splash.{h,cpp}  - pixel-art animation engine
power.{h,cpp}   - battery (if PMU present, otherwise stubbed)
touch_tt21100.{h,cpp} - touch driver
wifi_cfg.{h,cpp} - NVS-backed config: SSID, PSK, OAuth access/refresh/expires
portal.{h,cpp}  - first-boot AP + captive HTTP form
net.{h,cpp}     - STA connect + fallback to AP
oauth.{h,cpp}   - refresh access token via console.anthropic.com
poll.{h,cpp}    - periodic HEAD/POST /v1/messages, parses rate-limit headers → UsageData
data.h          - UsageData struct
icons.h, logo.h - image assets
font_*.c        - pre-compiled LVGL 9 bitmap fonts
splash_animations.h - generated, do not hand-edit
```

## Build / flash

```bash
pio run -d firmware -e s3box                                       # build
pio run -d firmware -e s3box -t upload --upload-port /dev/ttyACM0  # flash
```

Device shows up as `/dev/ttyACM0` (Espressif USB JTAG/serial debug unit).

## QA your own UI changes - don't ask the user

The firmware ships a `screenshot` serial command that dumps the LVGL framebuffer over `/dev/ttyACM0`. `./screenshot.sh out.png /dev/ttyACM0` captures a PNG. **Use this on every UI iteration** - Read the PNG with the Read tool, verify the change visually, iterate.

The boot screen is `SCREEN_SPLASH` and only advances on a physical button press. To screenshot the screen you're actually editing, **temporarily change the default boot screen** in `main.cpp` (search for `ui_show_screen(SCREEN_SPLASH);`) to `SCREEN_USAGE` / `SCREEN_NETWORK`, do your iteration, then revert before committing.

## Network architecture

**First boot / unconfigured:**
1. Device starts SoftAP `Clawdbox-setup` (open, no PSK).
2. Captive portal at `http://192.168.4.1/` collects SSID, WiFi password, OAuth tokens.
3. Tokens come from `~/.claude/.credentials.json` on the host (`accessToken`, `refreshToken`, `expiresAt`). User pastes once.
4. Saved to NVS via `Preferences`.
5. Reboot → STA mode.

**Normal boot:**
1. STA connect to saved SSID.
2. `oauth_refresh_if_needed()` runs on boot and every ~15 min: if `expiresAt` < now + 5 min, POST to `https://console.anthropic.com/v1/oauth/token` with `refresh_token`. Persist rotated tokens.
3. `poll_tick()` every 60s: POST `https://api.anthropic.com/v1/messages` with `Authorization: Bearer <accessToken>` + `anthropic-beta: oauth-2025-04-20`. Read response headers `anthropic-ratelimit-unified-5h-utilization`, `-5h-reset`, `-7d-utilization`, `-7d-reset`, `-5h-status`. Populate `UsageData`.

**Connection failure handling:**
- STA join fails 3× → fallback to AP/portal.
- OAuth refresh fails (network or 401) → show error on Network screen, retry with backoff.
- Long-press BOOT (>5s) → clear NVS, reboot to portal.

## TLS

`WiFiClientSecure` with pinned Anthropic root CA (ISRG Root X1). Do not `setInsecure()` in production.

## Critical gotchas

1. **OPI PSRAM** required: `board_build.arduino.memory_type = qio_opi` in platformio.ini.
2. **pioarduino platform required.** GFX Library for Arduino needs Arduino Core 3.x.
3. **LVGL 9 font patching.** `lv_font_conv` outputs LVGL 8 format. Must remove `#if LVGL_VERSION_MAJOR >= 8` guards, drop `.cache` field, add `.release_glyph`, `.kerning`, `.static_bitmap`, `.fallback`, `.user_data`.
4. **Token refresh on the device.** Claude Code CLI normally refreshes tokens in `~/.claude/.credentials.json`. The device cannot read that file - it must refresh itself with the `refreshToken` and persist the rotated pair. If refresh ever returns 400/401, the user must re-paste credentials in the portal.

## Icons

`tools/png_to_lvgl.js <input.png> <symbol> [W_MACRO] [H_MACRO] [--tint=RRGGBB | --no-tint]` converts an alpha PNG to RGB565A8. Default tint is white.

## Splash animations

13 × 20×20 pixel-art creature animations sourced from [claudepix.vercel.app](https://claudepix.vercel.app). Pipeline:

```bash
node tools/scrape_claudepix.js  # → tools/claudepix_data/*.json
node tools/convert_to_c.js      # → firmware/src/splash_animations.h
```

## User profile / preferences

See `~/.claude/projects/.../memory/` files for persistent context (embedded-beginner senior dev, brand-conscious, prefers iterative UI refinement, dislikes me authoring my own art when third-party assets are intended). Always read those memory files at session start.
