#!/bin/sh
# BSDJP - FreeBSD Japanese IME Setup Script
# Configures JP106 keyboard, installs dependencies, and sets up the environment

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== BSDJP FreeBSD Japanese Environment Setup ==="
echo ""

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: This script must be run as root."
    echo "Usage: sudo sh $0"
    exit 1
fi

# --- Console JP106 Keyboard ---
echo "[1/5] Configuring JP106 keyboard for console..."
if ! grep -q '^keymap="jp"' /etc/rc.conf 2>/dev/null; then
    if grep -q '^keymap=' /etc/rc.conf 2>/dev/null; then
        sed -i '' 's/^keymap=.*/keymap="jp"/' /etc/rc.conf
    else
        echo 'keymap="jp"' >> /etc/rc.conf
    fi
    echo "  -> Added keymap=\"jp\" to /etc/rc.conf"
else
    echo "  -> Already configured."
fi

# --- Locale ---
echo "[2/5] Configuring Japanese locale..."
LOCALE_LINE='japanese_still_not_configured="NO"'
if ! grep -q 'LANG.*ja_JP' /etc/login.conf 2>/dev/null; then
    cat >> /etc/profile <<'LOCALE_EOF'

# Japanese locale (BSDJP)
export LANG=ja_JP.UTF-8
export LC_ALL=ja_JP.UTF-8
export LC_CTYPE=ja_JP.UTF-8
LOCALE_EOF
    echo "  -> Added Japanese locale to /etc/profile"
else
    echo "  -> Japanese locale already configured."
fi

# --- Install Dependencies via pkg ---
echo "[3/5] Installing required packages..."
pkg install -y \
    xcb-imdkit \
    libxcb \
    xcb-util \
    xcb-util-keysyms \
    cairo \
    pango \
    cmake \
    pkgconf \
    xorg \
    ja-font-ipa \
    ja-font-ipa-uigothic \
    ja-font-takao

echo "  -> Packages installed."

# --- Xorg JP106 keyboard ---
echo "[4/5] Configuring Xorg for JP106 keyboard..."
XORG_KBD_CONF="/usr/local/etc/X11/xorg.conf.d/30-keyboard.conf"
mkdir -p "$(dirname "$XORG_KBD_CONF")"

cat > "$XORG_KBD_CONF" <<'XORG_EOF'
Section "InputClass"
    Identifier "Japanese Keyboard"
    MatchIsKeyboard "on"
    Option "XkbLayout" "jp"
    Option "XkbModel" "jp106"
    Option "XkbOptions" "terminate:ctrl_alt_bksp"
EndSection
XORG_EOF
echo "  -> Created $XORG_KBD_CONF"

# --- Download SKK dictionary ---
echo "[5/5] Downloading SKK dictionary..."
sh "$SCRIPT_DIR/download_dict.sh"

echo ""
echo "=== Setup complete ==="
echo "Reboot to apply console keymap changes."
echo "Run 'startx' or log into KDE Plasma to use the Xorg keyboard config."
echo ""
echo "To build BSDJP:"
echo "  cd $PROJECT_DIR"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make"
