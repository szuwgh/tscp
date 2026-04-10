#!/bin/bash
set -euo pipefail

TARFILE="openssl-1.1.1m.tar.gz"
if [ ! -f "$TARFILE" ]; then
	echo "Downloading $TARFILE..."
	if command -v curl >/dev/null 2>&1; then
		curl -fsSL -o "$TARFILE" "https://www.openssl.org/source/$TARFILE"
	else
		wget -q "https://www.openssl.org/source/$TARFILE" -O "$TARFILE"
	fi
else
	echo "$TARFILE already exists, skipping download."
fi

if [ ! -d "openssl-1.1.1m" ]; then
	tar -xvf "$TARFILE"
else
	echo "openssl-1.1.1m directory already exists, skipping extraction."
fi

cd openssl-1.1.1m
# 使用 musl cross 编译，并禁用 glibc 特有功能
export CC=x86_64-linux-musl-gcc
export AR=x86_64-linux-musl-ar
export RANLIB=x86_64-linux-musl-ranlib


# 禁用 _FORTIFY_SOURCE 以避免 __xxx_chk 符号被引用
export CFLAGS="-O3 -U_FORTIFY_SOURCE -DOPENSSL_NO_SECURE_MEMORY"
export LDFLAGS="-U_FORTIFY_SOURCE"

./Configure linux-x86_64 \
no-shared \
no-tests \
no-engine \
no-afalgeng \
--prefix="/home/unvdb/cproject/tscp/static/openssl" \
--openssldir="/home/unvdb/cproject/tscp/static/openssl/ssl"

make -j$(nproc)
make install

cd ..
rm -rf openssl-1.1.1m openssl-1.1.1m.tar.gz