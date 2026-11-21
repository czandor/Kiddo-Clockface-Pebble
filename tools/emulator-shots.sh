#!/usr/bin/env bash
# Emulator screenshots for the docs: tools/emulator-shots.sh basalt chalk emery gabbro (run in the NineTimeSDK WSL).
# Writes docs/<platform>-24h.png, -second-board.png, -12h.png and -12h-am.png from the newest dist/Kiddo-*.pbw.
set -u
export PATH=/opt/ninetime-cli/bin:/usr/local/bin:/usr/bin:/bin
cd "$(dirname "$0")/../pebble"
PBW=$(ls -t ../dist/Kiddo-*.pbw | head -1)
for P in "$@"; do
  echo "== $P ($PBW)"
  timeout 300 pebble install --emulator "$P" "$PBW" 2>&1 | grep -E "succeeded|rror" | head -3
  pebble emu-time-format --emulator "$P" --format 24h >/dev/null 2>&1
  pebble emu-set-time --emulator "$P" 20:08:00 >/dev/null 2>&1
  sleep 3
  pebble screenshot --no-open --emulator "$P" "../docs/$P-24h.png" >/dev/null 2>&1
  pebble emu-tap --emulator "$P" >/dev/null 2>&1
  sleep 2
  pebble screenshot --no-open --emulator "$P" "../docs/$P-second-board.png" >/dev/null 2>&1
  pebble emu-tap --emulator "$P" >/dev/null 2>&1
  pebble emu-time-format --emulator "$P" --format 12h >/dev/null 2>&1
  pebble emu-set-time --emulator "$P" 16:06:00 >/dev/null 2>&1
  sleep 3
  pebble screenshot --no-open --emulator "$P" "../docs/$P-12h.png" >/dev/null 2>&1
  pebble emu-set-time --emulator "$P" 10:00:00 >/dev/null 2>&1
  sleep 3
  pebble screenshot --no-open --emulator "$P" "../docs/$P-12h-am.png" >/dev/null 2>&1
  ls -la ../docs/$P-*.png
done
