#include "qr_render.h"
#include "qrcode.h"
#include <esp_heap_caps.h>

static lv_image_dsc_t dsc = {};
static uint16_t* pix = nullptr;
static int last_size = 0;

// ricmoo/QRCode divides by zero (instead of returning an error) when the
// payload exceeds the chosen version's capacity. Pre-validate text length
// against the ECC_LOW byte-mode capacity table before calling into the lib.
#define QR_MIN_VERSION 1
#define QR_MAX_VERSION 20
// qrcode_getBufferSize(20) = ((20*4+17)*(20*4+17)+7)/8 = 1177
#define QR_BUF_BYTES_V20 1177

// ECC_LOW byte-mode capacity per version (QR spec). Index 0 unused.
static const uint16_t kCapLow[21] = {
    0,
    17,  32,  53,  78, 106, 134, 154, 192, 230, 271,
    321, 367, 425, 458, 520, 586, 644, 718, 792, 858,
};

bool qr_render(const char* text, int target_px) {
    qr_free();
    if (!text) return false;
    size_t tlen = strlen(text);
    if (tlen == 0 || tlen > kCapLow[QR_MAX_VERSION]) return false;

    // Buffer sized for the largest version we'll try; reused across attempts.
    static uint8_t bytes[QR_BUF_BYTES_V20];
    QRCode qr;
    int chosen_ver = 0;
    for (int v = QR_MIN_VERSION; v <= QR_MAX_VERSION; v++) {
        if (tlen > kCapLow[v]) continue;
        if (qrcode_initText(&qr, bytes, v, ECC_LOW, text) == 0 && qr.size > 0) {
            chosen_ver = v;
            break;
        }
    }
    if (!chosen_ver || qr.size == 0) return false;

    const int quiet = 4;
    int mods  = qr.size;
    int total = mods + 2 * quiet;
    int s = target_px / total;
    if (s < 1) s = 1;
    int px_total = total * s;

    size_t buf_bytes = (size_t)px_total * px_total * 2;
    pix = (uint16_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    if (!pix) return false;
    for (int i = 0; i < px_total * px_total; i++) pix[i] = 0xFFFF;

    for (int y = 0; y < mods; y++) {
        for (int x = 0; x < mods; x++) {
            if (!qrcode_getModule(&qr, x, y)) continue;
            int px0 = (quiet + x) * s;
            int py0 = (quiet + y) * s;
            for (int dy = 0; dy < s; dy++) {
                uint16_t* row = pix + (py0 + dy) * px_total + px0;
                for (int dx = 0; dx < s; dx++) row[dx] = 0x0000;
            }
        }
    }

    dsc.header.w = px_total;
    dsc.header.h = px_total;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.stride = px_total * 2;
    dsc.data = (const uint8_t*)pix;
    dsc.data_size = buf_bytes;
    last_size = px_total;
    return true;
}

const lv_image_dsc_t* qr_get_image(void) { return &dsc; }
int qr_get_size_px(void) { return last_size; }
void qr_free(void) {
    if (pix) {
        heap_caps_free(pix);
        pix = nullptr;
    }
    last_size = 0;
    dsc = {};
}
