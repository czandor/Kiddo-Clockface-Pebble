#!/usr/bin/env bash
set -euo pipefail

quality="${1:-excellent}"
bpm="${2:-72}"
case "$quality" in excellent|poor|off-wrist) ;; *) exit 2 ;; esac
if [[ ! "$bpm" =~ ^[0-9]{1,3}$ ]] || (( 10#$bpm > 255 )); then
  exit 2
fi

samples=14
if [[ "$quality" == off-wrist ]]; then
  samples=1
  bpm=0
fi

for (( sample=1; sample<=samples; sample++ )); do
  /opt/ninetime-cli/bin/pebble emu-heart-rate --emulator emery --quality "$quality" "$bpm"
  if (( sample < samples )); then sleep 0.6; fi
done
