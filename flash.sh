#!/bin/bash
# Build and flash firmware to an ESP32-S3-BOX variant.
# Usage: ./flash.sh [s3box|s3box_lite|s3box3] [port]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV="${1:-s3box}"
PORT="${2:-/dev/ttyACM0}"

case "$ENV" in
  s3box|s3box_lite|s3box3) ;;
  *)
    echo "Unknown env '$ENV'. Use: s3box, s3box_lite, s3box3"
    exit 1
    ;;
esac

echo "=== Flashing Clawdbox ==="
echo "Env:  $ENV"
echo "Port: $PORT"
echo ""

cd "$SCRIPT_DIR/firmware"
~/.platformio/penv/bin/pio run -e "$ENV" -t upload --upload-port "$PORT"

echo ""
echo "=== Done! ==="
