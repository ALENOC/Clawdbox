# Clawdmeter

A small ESP32 dashboard I made for my desk to keep an eye on Claude Code usage.

It runs on an **Espressif ESP32-S3-BOX**, connects to your WiFi, and polls the Anthropic API directly for rate-limit utilization. No host daemon required.

The splash screen plays pixel-art Clawd animations that get busier when your usage rate climbs.

The Clawd animations come from [claudepix](https://claudepix.vercel.app), [@amaanbuilds](https://x.com/amaanbuilds)'s library of pixel-art Clawd sprites, check it out, it's lovely.

## Screens

The device boots into the splash and stays there until you press the BOOT button, which cycles between Usage and Network. Tap the screen anywhere (except the Reset zone on the Network screen) to flip back to the splash; tap again to dismiss it.

While the splash is up, BOOT cycles animations instead of screens.

## Hardware

- Espressif ESP32-S3-BOX (320×240 IPS, ILI9342C, TT21100 cap touch)
- USB-C cable for flashing

## Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html)
- Claude Code with an active subscription (for OAuth tokens)

## Flash the firmware

```bash
cd firmware
pio run -e s3box -t upload --upload-port /dev/ttyACM0
```

Or use the helper:

```bash
./flash.sh /dev/ttyACM0
```

## First-boot setup

1. After a fresh flash (or after a config reset), the device starts a WiFi access point named **`Clawdmeter-setup`** (open, no password).
2. Connect a phone or laptop to that AP. Most OSes auto-open the captive portal at `http://192.168.4.1/`; if not, browse there manually.
3. Fill the form:
   - **SSID / Password** — your home WiFi
   - **accessToken / refreshToken / expiresAt** — copy from `~/.claude/.credentials.json` on a host that's logged in to Claude Code
4. Submit. The device reboots and connects to your WiFi.
5. From then on, the device refreshes its OAuth token autonomously every ~hour.

## Reset

- **Reset config** tap zone on the Network screen — clears NVS and reboots into setup AP.
- Or long-press the **BOOT** button for >5 seconds.

## Security note

OAuth tokens are stored in NVS in plaintext. The device has no remote attack surface, but anyone with USB access can extract NVS contents with `esptool.py read_flash`. **Before lending, gifting, or recycling the device, run a config reset** (long-press BOOT >5s or use the Reset config tap zone) and revoke the token at <https://console.anthropic.com>.

## How it works

1. Firmware connects to WiFi using credentials saved in NVS.
2. NTP sync sets the clock (needed for OAuth expiry math).
3. Every 60 s, the device POSTs a 1-token probe to `https://api.anthropic.com/v1/messages` and reads rate-limit headers (`anthropic-ratelimit-unified-5h-utilization` and friends) — same trick the host daemon used to play.
4. When the saved access token nears expiry, the device hits `https://console.anthropic.com/v1/oauth/token` with its refresh token and persists the rotated pair.
5. UI tracks the rate of change of session % over a sliding window and picks splash animations from the matching mood group.

## Physical buttons

| Button       | GPIO   | Function                                                       |
| ------------ | ------ | -------------------------------------------------------------- |
| **BOOT**     | GPIO 0 | Cycle screens / advance splash; long-press (>5s) → reset config |
| **Mute slider** | GPIO 1 | Manual poll trigger                                            |

## Recompiling fonts

The `firmware/src/font_*.c` files are pre-compiled LVGL bitmap fonts.

```bash
npm install -g lv_font_conv
```

Generate with `--no-compress` (required for LVGL 9):

```bash
for size in 28 20; do
  lv_font_conv --font assets/StyreneB-Regular.otf -r 0x20-0x7E \
    --size $size --format lvgl --bpp 4 --no-compress \
    -o firmware/src/font_styrene_${size}.c --lv-include "lvgl.h"
done
```

**Important:** `lv_font_conv` outputs LVGL 8 format. Each generated file must be patched for LVGL 9:

1. Remove `#if LVGL_VERSION_MAJOR >= 8` guards
2. Remove the `.cache` field from `font_dsc`
3. Add `.release_glyph = NULL`, `.kerning = 0`, `.static_bitmap = 0`, `.fallback = NULL`, `.user_data = NULL` to the font struct

Without these patches, fonts compile but render invisible.

## Converting Lucide icons

```bash
node tools/png_to_lvgl.js assets/icon_bluetooth_48.png icon_bluetooth_data ICON_BLUETOOTH_WIDTH ICON_BLUETOOTH_HEIGHT
```

Default tint is white (`0xFFFFFF`); Lucide PNGs ship as black-on-transparent. Pass `--no-tint` for pre-coloured artwork.

## Splash animations

Animations come from [claudepix.vercel.app](https://claudepix.vercel.app):

```bash
node tools/scrape_claudepix.js
node tools/convert_to_c.js
pio run -d firmware -t upload
```

See `tools/README.md` for details.

## Credits

- Pixel-art Clawd animations by [@amaanbuilds](https://x.com/amaanbuilds), sourced from [claudepix.vercel.app](https://claudepix.vercel.app).
- Lucide icon set ([lucide.dev](https://lucide.dev), MIT).
- Anthropic brand fonts (Tiempos Text, Styrene B) — see licensing warning below.

## Licensing gray area warning

The software in this repository uses and adheres to the Anthropic brand guidelines and uses the same proprietary fonts that Anthropic has a license for but this software uses without permission as well as using assets from Anthropic such as the copyrighted Clawd mascot, so even though the code in this repo is non-proprietary I will not license it under a copyleft license since this repo includes proprietary fonts and copyrighted assets. Please be aware of this if you fork or copy the code from this repo. **You have been warned!**
