# 编译器选择
CC = x86_64-linux-musl-gcc

# 编译选项
# -Wall: 启用所有警告
# -g: 包含调试信息
# -I.: 包含当前目录的头文件
# 编译选项
# libssh2头文件目录，按实际路径改
CFLAGS = -O2 -static -I. \
	-I../static/libssh2/include \
	-I../static/openssl/include \
	-I../static/zlib/include      
# 链接选项
LDFLAGS = -static -L../static/libssh2/lib # libssh2库目录
LDFLAGS += -L../static/openssl/lib # OpenSSL库目录
LDFLAGS += -L../static/zlib/lib    # 只连接zlib静态库
# 链接的静态库
# 显式静态库
LIBS = \
../static/libssh2/lib/libssh2.a \
../static/openssl/lib/libssl.a \
../static/openssl/lib/libcrypto.a \
../static/zlib/lib/libz.a

# 获取当前目录下所有.c文件
SRCS = $(wildcard *.c)

# 将.c文件列表转换为对应的.o目标文件列表
OBJS = $(SRCS:.c=.o)

# 最终生成的可执行文件名称，默认为main
TARGET = tscp

# 默认目标：生成可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# 通配规则：说明如何将.c文件编译为.o文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理编译生成的文件
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)

# 伪目标：重新编译
.PHONY: rebuild
rebuild: clean $(TARGET)