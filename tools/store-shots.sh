#!/usr/bin/env bash
# Store screenshots with realistic health data: tools/store-shots.sh (run in the NineTimeSDK WSL).
# Writes store/<platform>-NN-*.png from the newest dist/Kiddo-*.pbw.
set -u
export PATH=/opt/ninetime-cli/bin:/usr/local/bin:/usr/bin:/bin
cd "$(dirname "$0")/../pebble"
PBW=$(ls -t ../dist/Kiddo-*.pbw | head -1)
OUT=../store
mkdir -p "$OUT"

shot() {  # platform name time format [tap]
  local E="--emulator $1"
  pebble emu-time-format $E --format "$4" >/dev/null 2>&1
  pebble emu-set-time $E "$3" >/dev/null 2>&1
  sleep 2
  if [[ "${5:-}" == tap ]]; then
    pebble emu-tap $E >/dev/null 2>&1
    sleep 1.5
  fi
  pebble screenshot --no-open $E "$OUT/$1-$2.png" >/dev/null 2>&1
  if [[ "${5:-}" == tap ]]; then pebble emu-tap $E >/dev/null 2>&1; fi
}

for P in basalt chalk emery gabbro; do
  E="--emulator $P"
  timeout 300 pebble install $E "$PBW" 2>&1 | grep -E "succeeded|rror"
  pebble emu-battery $E --percent 80 >/dev/null 2>&1
  pebble emu-steps $E 8135 >/dev/null 2>&1
  pebble emu-distance $E 6020 >/dev/null 2>&1
  pebble emu-active-time $E 42 >/dev/null 2>&1
  pebble emu-calories $E --resting 1480 465 >/dev/null 2>&1
  pebble emu-sleep $E 452 >/dev/null 2>&1
  if [[ $P == emery ]]; then
    for i in 1 2 3 4 5 6; do pebble emu-heart-rate $E --quality excellent 72 >/dev/null 2>&1; sleep 0.6; done
  fi
  shot $P 01-main 10:08:00 24h
  shot $P 02-second-board 16:04:00 24h tap
  shot $P 03-12h 09:41:00 12h
  echo "$P done"
done
