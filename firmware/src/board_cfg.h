#pragma once

// Board-specific hardware configuration for all ESP32-S3-BOX variants.
// Select via PlatformIO build_flags: -DBOARD_S3BOX, -DBOARD_S3BOX_LITE, or -DBOARD_S3BOX_3
//
// Pin reference (Espressif esp-bsp / esp-box repositories, 2026-05-17):
//   github.com/espressif/esp-bsp  —  bsp/esp-box / esp-box-lite / esp-box-3

// ---- Common pins (identical on all three variants) ----
#define LCD_WIDTH    320
#define LCD_HEIGHT   240
#define LCD_CS       5
#define LCD_DC       4
#define LCD_SCLK     7
#define LCD_MOSI     6
#define LCD_RESET    48

#define IIC_SDA      8
#define IIC_SCL      18
#define TP_INT       3

#define TT21100_ADDR  0x24
#define GT911_ADDR_A  0x5D
#define GT911_ADDR_B  0x14

#define BTN_BOOT     0
#define BTN_MUTE     1

// ---- Display type tokens ----
#define DISPLAY_ILI9342C  1   // native landscape 320x240, no inversion
#define DISPLAY_ST7789    2   // portrait native, rotated 90°, IPS inversion on

// ---- Board-specific overrides ----
#if defined(BOARD_S3BOX)
  // ESP32-S3-BOX (original, ~2022)
  // Display: ILI9342C, native landscape, backlight GPIO45
  // Touch:   TT21100 (X axis mirrored relative to panel)
  #define LCD_BL             45
  #define BOARD_DISPLAY_TYPE DISPLAY_ILI9342C
  #define BOARD_HAS_TOUCH    1
  #define BOARD_HAS_GT911    0

#elif defined(BOARD_S3BOX_LITE)
  // ESP32-S3-BOX-Lite (~2022, no touch, 3 mechanical buttons)
  // Display: ST7789, backlight GPIO45
  // Touch:   none
  #define LCD_BL             45
  #define BOARD_DISPLAY_TYPE DISPLAY_ST7789
  #define BOARD_HAS_TOUCH    0
  #define BOARD_HAS_GT911    0

#elif defined(BOARD_S3BOX_3)
  // ESP32-S3-BOX-3 (~2023, latest)
  // Backlight: GPIO47 (swapped vs original — GPIO45 is I2S_LCLK on BOX-3)
  // Display: ST7789 when TT21100 touch present; ILI9342C-compat when GT911 present
  //          (mirrors Espressif BSP auto-detect logic)
  // Touch:   TT21100 (older panel revision) or GT911 (newer panel revision)
  //          — detected at runtime via I2C probe
  #define LCD_BL             47
  #define BOARD_DISPLAY_TYPE DISPLAY_ST7789   // default; may be overridden to ILI9342C at runtime
  #define BOARD_HAS_TOUCH    1
  #define BOARD_HAS_GT911    1                // probe GT911 at runtime

#else
  #error "No board target. Add -DBOARD_S3BOX, -DBOARD_S3BOX_LITE, or -DBOARD_S3BOX_3 to build_flags."
#endif
