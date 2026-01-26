#!/bin/bash
set -euo pipefail

# 判断 zlib-1.2.13.tar.gz 是否存在 否则下载
TARFILE="zlib-1.2.13.tar.gz"
if [ ! -f "$TARFILE" ]; then
	echo "Downloading $TARFILE..."
	if command -v curl >/dev/null 2>&1; then
		curl -fsSL -o "$TARFILE" "https://zlib.net/fossils/$TARFILE"
	else
		wget -q "https://zlib.net/fossils/$TARFILE" -O "$TARFILE"
	fi
else
	echo "$TARFILE already exists, skipping download."
fi

if [ ! -d "zlib-1.2.13" ]; then
	tar -xvf "$TARFILE"
else
	echo "zlib-1.2.13 directory already exists, skipping extraction."
fi

cd zlib-1.2.13

# Use musl cross-toolchain for a static build
export CC=${CC:-x86_64-linux-musl-gcc}
export AR=${AR:-x86_64-linux-musl-ar}
export RANLIB=${RANLIB:-x86_64-linux-musl-ranlib}

./configure --static --prefix="/home/postgres/cproject/tscp/static/zlib"

make -j$(nproc)
make install

cd ..
rm -rf zlib-1.2.13 zlib-1.2.13.tar.gz