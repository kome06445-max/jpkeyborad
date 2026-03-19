#!/bin/sh
# Download SKK dictionaries for BSDJP

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DATA_DIR="$(dirname "$SCRIPT_DIR")/data"

SKK_BASE_URL="https://raw.githubusercontent.com/skk-dev/dict/master"
DICT_FILE="$DATA_DIR/SKK-JISYO.L"

mkdir -p "$DATA_DIR"

if [ -f "$DICT_FILE" ]; then
    echo "  -> SKK dictionary already exists at $DICT_FILE"
    exit 0
fi

echo "  -> Downloading SKK-JISYO.L (large dictionary)..."

if command -v fetch >/dev/null 2>&1; then
    fetch -o "$DICT_FILE" "$SKK_BASE_URL/SKK-JISYO.L"
elif command -v curl >/dev/null 2>&1; then
    curl -fSL -o "$DICT_FILE" "$SKK_BASE_URL/SKK-JISYO.L"
elif command -v wget >/dev/null 2>&1; then
    wget -O "$DICT_FILE" "$SKK_BASE_URL/SKK-JISYO.L"
else
    echo "Error: No download tool available (fetch, curl, or wget required)"
    exit 1
fi

echo "  -> Dictionary saved to $DICT_FILE"
