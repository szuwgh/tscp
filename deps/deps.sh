#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "➡️  Building dependencies in $SCRIPT_DIR"

echo "1) Building zlib..."
./zlib.sh

echo "2) Building OpenSSL..."
./openssl.sh

echo "3) Building libssh2..."
./libssh2.sh

echo "4) Building ncurses..."
./ncurses.sh

echo "✅ All dependencies built and installed to ../static"

