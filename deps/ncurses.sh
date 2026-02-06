#!/bin/bash
set -euo pipefail

# 设置 ncurses 版本和文件名
NCURSES_VERSION="6.4"
TARFILE="ncurses-${NCURSES_VERSION}.tar.gz"
DOWNLOAD_URL="https://ftp.gnu.org/pub/gnu/ncurses/${TARFILE}"

# 判断 ncurses 源码包是否存在，否则下载
if [ ! -f "$TARFILE" ]; then
    echo "Downloading $TARFILE..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$TARFILE" "$DOWNLOAD_URL"
    else
        wget -q "$DOWNLOAD_URL" -O "$TARFILE"
    fi
else
    echo "$TARFILE already exists, skipping download."
fi

# 解压源码包
if [ ! -d "ncurses-${NCURSES_VERSION}" ]; then
    tar -xvf "$TARFILE"
else
    echo "ncurses-${NCURSES_VERSION} directory already exists, skipping extraction."
fi

cd "ncurses-${NCURSES_VERSION}"

# 设置编译环境（根据您的需求调整）
export CC=x86_64-linux-musl-gcc
export AR=x86_64-linux-musl-ar
export RANLIB=x86_64-linux-musl-ranlib

# 配置编译选项
./configure \
    --prefix="/home/postgres/cproject/tscp/static/ncurses" \
    --without-shared \
    --with-static \
    --enable-widec \
    --with-normal \
    --without-debug \
    --without-ada \
    --without-progs \
    --without-tests

# 编译并安装
make -j$(nproc)
make install

cd ..

# 可选：清理源码目录和压缩包
# rm -rf "ncurses-${NCURSES_VERSION}" "$TARFILE"

echo "ncurses static library compilation completed!"

cd ..
rm -rf ncurses-6.4 ncurses-6.4.tar.gz