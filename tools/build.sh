#!/usr/bin/env bash
set -euo pipefail
export PATH="/opt/ninetime-cli/bin:/usr/local/bin:/usr/bin:/bin"
cd "$(dirname "$0")/../pebble"
pebble build
