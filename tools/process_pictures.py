#!/usr/bin/env python3
"""Crop device photos, upscale, sharpen, blur SSID in network.jpeg."""
import math
from pathlib import Path
from PIL import Image, ImageFilter, ImageDraw

PICS = Path("pictures")

# Crop to device+dock only (remove wall + table) — coords in orig 1200×1600
CROP = (65, 108, 1145, 1290)  # left, top, right, bottom

def rotated_rect_polygon(cx, cy, w, h, angle_deg):
    """Return 4 corners of a rotated rectangle."""
    a = math.radians(angle_deg)
    cos_a, sin_a = math.cos(a), math.sin(a)
    hw, hh = w / 2, h / 2
    corners = [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]
    return [
        (cx + cos_a * x - sin_a * y, cy + sin_a * x + cos_a * y)
        for x, y in corners
    ]

def blur_region(img, polygon, radius=28):
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=255)
    blurred = img.filter(ImageFilter.GaussianBlur(radius=radius))
    out = img.copy()
    out.paste(blurred, mask=mask)
    return out

for name in ("splash", "usage", "settings", "network"):
    src = PICS / f"{name}.jpeg"
    img = Image.open(src)
    print(f"{name}: {img.size}")

    # 1. Crop to device
    img = img.crop(CROP)

    # 2. Upscale 2× Lanczos
    w, h = img.size
    img = img.resize((w * 2, h * 2), Image.LANCZOS)

    # 3. Unsharp mask — sharpens without noise amplification
    img = img.filter(ImageFilter.UnsharpMask(radius=1.8, percent=140, threshold=3))

    # 4. Blur SSID for network screen
    if name == "network":
        # "SSID: YOUR_SSID" in orig image: y≈488-535, x≈220-690
        # After crop(65,108): x→155-625, y→380-427
        # After 2× upscale:   x→310-1250, y→760-854
        # Center: (780, 807)  size: 940×94  tilt: -2.5° (screen angled back)
        cx, cy = 780, 807
        poly = rotated_rect_polygon(cx, cy, w=940, h=94, angle_deg=-2.5)
        img = blur_region(img, poly, radius=28)
        print("  -> SSID blurred")

    img.save(src, "JPEG", quality=92, optimize=True)
    print(f"  -> saved ({img.size[0]}×{img.size[1]})")
