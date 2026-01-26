#!/bin/bash
set -euo pipefail

# Run from the script directory so relative paths behave correctly

TARFILE="libssh2-1.11.0.tar.gz"
if [ ! -f "$TARFILE" ]; then
	echo "Downloading $TARFILE..."
	if command -v curl >/dev/null 2>&1; then
		curl -fsSL -o "$TARFILE" "https://www.libssh2.org/download/$TARFILE"
	else
		wget -q "https://www.libssh2.org/download/$TARFILE" -O "$TARFILE"
	fi
else
	echo "$TARFILE already exists, skipping download."
fi

if [ ! -d "libssh2-1.11.0" ]; then
	tar -xvf "$TARFILE"
else
	echo "libssh2-1.11.0 directory already exists, skipping extraction."
fi

cd libssh2-1.11.0

export CC=x86_64-linux-musl-gcc
export AR=x86_64-linux-musl-ar
export RANLIB=x86_64-linux-musl-ranlib

if ! command -v "$CC" >/dev/null 2>&1; then 
		echo "Error: compiler '$CC' not found in PATH. Please install musl cross toolchain or set CC to a valid compiler." >&2
		exit 1
fi

echo '# musl-toolchain.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 交叉编译器
set(CMAKE_C_COMPILER /usr/local/x86_64-linux-musl-cross/bin/x86_64-linux-musl-gcc)
set(CMAKE_AR /usr/local/x86_64-linux-musl-cross/bin/x86_64-linux-musl-ar)
set(CMAKE_RANLIB /usr/local/x86_64-linux-musl-cross/bin/x86_64-linux-musl-ranlib)

# 查找库/头路径
set(CMAKE_FIND_ROOT_PATH
/home/postgres/cproject/tscp/static/openssl
/home/postgres/cproject/tscp/static/zlib
)

# 查找规则
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


# 强制静态链接
set(CMAKE_EXE_LINKER_FLAGS "-static")' > musl-toolchain.cmake

cmake \
    -DCMAKE_TOOLCHAIN_FILE="./musl-toolchain.cmake" \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="../../static/libssh2" \
	-DCRYPTO_BACKEND=OpenSSL \
	-DOPENSSL_ROOT_DIR="/home/postgres/cproject/tscp/static/openssl" \
	-DOPENSSL_INCLUDE_DIR="/home/postgres/cproject/tscp/static/openssl/include" \
	-DOPENSSL_SSL_LIBRARY="/home/postgres/cproject/tscp/static/openssl/lib/libssl.a" \
	-DOPENSSL_CRYPTO_LIBRARY="/home/postgres/cproject/tscp/static/openssl/lib/libcrypto.a" \
	-DOPENSSL_USE_STATIC_LIBS=TRUE \
	-DZLIB_INCLUDE_DIR="/home/postgres/cproject/tscp/static/zlib/include" \
	-DZLIB_LIBRARY="/home/postgres/cproject/tscp/static/zlib/lib/libz.a" \
	-DENABLE_ZLIB_COMPRESSION=ON \
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_TESTING=OFF 

echo "🔨 Build"
cmake --build . -- -j$(nproc)

echo "📥 Install"
cmake --install .

cd ../
rm -rf libssh2-1.11.0 libssh2-1.11.0.tar.gz