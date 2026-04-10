#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#define MAX_BUFFER_SIZE 1024 * 1024 // 1MB buffer for transfer

typedef struct
{
    char *host;
    int port;
    char *username;
    char *password;
    char *local_path;
    char *remote_path;
    int operation; // 0: upload, 1: download
    int verbose;
} config_t;

// 显示使用说明
void print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("SSH File Transfer Tool using libssh2\n\n");
    printf("Options:\n");
    printf("  -h, --host HOST          Remote hostname or IP address (required)\n");
    printf("  -P, --port PORT          SSH port (default: 22)\n");
    printf("  -u, --user USERNAME      SSH username (required)\n");
    printf("  -p, --password PASSWORD  SSH password (required)\n");
    printf("  -l, --local LOCAL_PATH   Local file path (required)\n");
    printf("  -r, --remote REMOTE_PATH Remote file path (required)\n");
    printf("  -d, --download           Download file (default: upload)\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  --help                   Show this help message\n\n");
    printf("Examples:\n");
    printf("  Upload: %s -h 192.168.1.100 -u root -p password -l local.txt -r /tmp/remote.txt\n", program_name);
    printf("  Download: %s -h 192.168.1.100 -u root -p password -l local.txt -r /tmp/remote.txt -d\n", program_name);
}

// 初始化配置
void init_config(config_t *config)
{
    config->host = NULL;
    config->port = 22;
    config->username = NULL;
    config->password = NULL;
    config->local_path = NULL;
    config->remote_path = NULL;
    config->operation = 0; // default to upload
    config->verbose = 0;
}

// 解析命令行参数
int parse_arguments(int argc, char *argv[], config_t *config)
{
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'P'},
        {"user", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'p'},
        {"local", required_argument, 0, 'l'},
        {"remote", required_argument, 0, 'r'},
        {"download", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 0},
        {0, 0, 0, 0}};

    int c;
    while ((c = getopt_long(argc, argv, "h:P:u:p:l:r:dv", long_options, NULL)) != -1)
    {
        switch (c)
        {
        case 'h':
            config->host = strdup(optarg);
            break;
        case 'P':
            config->port = atoi(optarg);
            break;
        case 'u':
            config->username = strdup(optarg);
            break;
        case 'p':
            config->password = strdup(optarg);
            break;
        case 'l':
            config->local_path = strdup(optarg);
            break;
        case 'r':
            config->remote_path = strdup(optarg);
            break;
        case 'd':
            config->operation = 1; // download
            break;
        case 'v':
            config->verbose = 1;
            break;
        case 0: // help
            print_usage(argv[0]);
            exit(0);
        default:
            return -1;
        }
    }

    // 验证必需参数
    if (!config->host || !config->username || !config->password ||
        !config->local_path || !config->remote_path)
    {
        fprintf(stderr, "Error: Missing required parameters\n");
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}

// 建立socket连接
int connect_to_host(const char *host, int port)
{
    int sock;
    struct sockaddr_in sin;
    struct hostent *he;

    if ((he = gethostbyname(host)) == NULL)
    {
        fprintf(stderr, "Error: Could not resolve hostname %s\n", host);
        return -1;
    }

    // 检查地址类型，只处理IPv4地址[2,6]
    if (he->h_addrtype != AF_INET)
    {
        fprintf(stderr, "Error: Unsupported address type (only IPv4 supported)\n");
        return -1;
    }

    // 循环遍历h_addr_list中的每个地址[2,5]
    int i;
    for (i = 0; he->h_addr_list[i] != NULL; i++)
    {
        // 创建Socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
            perror("Error creating socket");
            continue;
        }

        // 设置地址结构体
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);

        // 复制当前IP地址到sockaddr_in结构[6,8]
        memcpy(&sin.sin_addr, he->h_addr_list[i], he->h_length);

        // 将二进制IP转换为可读格式用于调试[4,8]
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, he->h_addr_list[i], ip_str, sizeof(ip_str));
        printf("尝试连接 %s:%d (IP: %s)\n", host, port, ip_str);

        // 尝试连接
        if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) == 0)
        {
            printf("成功连接到 %s:%d (IP: %s)\n", host, port, ip_str);
            return sock;
        }
        else
        {
            perror("连接失败");
            close(sock);
            sock = -1;
        }
    }

    return sock;
}

// 显示传输进度
void show_progress(off_t transferred, off_t total, const char *operation)
{
    if (total == 0)
        return;

    int percentage = (int)((transferred * 100) / total);
    printf("\r%s: %3d%% [%ld/%ld bytes]", operation, percentage, transferred, total);
    fflush(stdout);

    if (transferred >= total)
    {
        printf("\n");
    }
}

// 上传文件到远程服务器
int upload_file(config_t *config, LIBSSH2_SESSION *session)
{
    FILE *local = NULL;
    LIBSSH2_CHANNEL *channel = NULL;
    struct stat file_info;
    char buffer[MAX_BUFFER_SIZE];
    size_t nread;
    int rc;

    printf("Uploading %s to %s:%s\n", config->local_path, config->host, config->remote_path);

    // 打开本地文件
    local = fopen(config->local_path, "rb");
    if (!local)
    {
        perror("Error opening local file");
        return -1;
    }

    // 获取文件信息
    if (stat(config->local_path, &file_info) != 0)
    {
        perror("Error getting file info");
        fclose(local);
        return -1;
    }

    // 发送SCP文件
    channel = libssh2_scp_send64(session, config->remote_path, file_info.st_mode & 0777,
                                 (libssh2_int64_t)file_info.st_size, 0, 0);
    if (!channel)
    {
        fprintf(stderr, "Error opening SCP channel for upload\n");
        fclose(local);
        return -1;
    }

    // 传输文件内容
    off_t transferred = 0;
    while ((nread = fread(buffer, 1, sizeof(buffer), local)) > 0)
    {
        char *ptr = buffer;

        while (nread > 0)
        {
            rc = libssh2_channel_write(channel, ptr, nread);
            if (rc < 0)
            {
                fprintf(stderr, "Error writing to channel: %d\n", rc);
                libssh2_channel_free(channel);
                fclose(local);
                return -1;
            }

            ptr += rc;
            nread -= rc;
            transferred += rc;

            if (config->verbose)
            {
                show_progress(transferred, file_info.st_size, "Uploading");
            }
        }
    }

    if (config->verbose)
    {
        show_progress(file_info.st_size, file_info.st_size, "Upload complete");
    }

    // 清理
    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);
    fclose(local);

    printf("File uploaded successfully\n");
    return 0;
}

// 从远程服务器下载文件
int download_file(config_t *config, LIBSSH2_SESSION *session)
{
    FILE *local = NULL;
    LIBSSH2_CHANNEL *channel = NULL;
    libssh2_struct_stat file_info;
    char buffer[MAX_BUFFER_SIZE];
    int rc;

    printf("Downloading %s:%s to %s\n", config->host, config->remote_path, config->local_path);

    // 打开SCP通道接收文件
    channel = libssh2_scp_recv2(session, config->remote_path, &file_info);
    if (!channel)
    {
        fprintf(stderr, "Error opening SCP channel for download\n");
        return -1;
    }

    // 创建本地文件
    local = fopen(config->local_path, "wb");
    if (!local)
    {
        perror("Error creating local file");
        libssh2_channel_free(channel);
        return -1;
    }

    // 接收文件内容
    off_t transferred = 0;
    libssh2_int64_t total_size = file_info.st_size;

    while (transferred < total_size)
    {
        rc = libssh2_channel_read(channel, buffer, sizeof(buffer));
        if (rc < 0)
        {
            fprintf(stderr, "Error reading from channel: %d\n", rc);
            break;
        }
        else if (rc == 0)
        {
            // EOF
            break;
        }

        size_t written = fwrite(buffer, 1, rc, local);
        if (written != rc)
        {
            perror("Error writing to local file");
            break;
        }

        transferred += rc;

        if (config->verbose)
        {
            show_progress(transferred, total_size, "Downloading");
        }
    }

    if (config->verbose && transferred >= total_size)
    {
        show_progress(total_size, total_size, "Download complete");
    }

    // 清理
    libssh2_channel_free(channel);
    fclose(local);

    printf("File downloaded successfully\n");
    return 0;
}

// 主函数
int main(int argc, char *argv[])
{
    config_t config;
    LIBSSH2_SESSION *session = NULL;
    int sock = -1;
    int rc;

    // 初始化配置
    init_config(&config);

    // 解析命令行参数
    if (parse_arguments(argc, argv, &config) != 0)
    {
        return 1;
    }
    printf("SSH File Transfer Tool using libssh2\n");
    // 初始化libssh2
    rc = libssh2_init(0);
    if (rc != 0)
    {
        fprintf(stderr, "Error initializing libssh2: %d\n", rc);
        return 1;
    }
    printf("libssh2 initialized\n");
    // 建立socket连接
    sock = connect_to_host(config.host, config.port);
    if (sock == -1)
    {
        libssh2_exit();
        return 1;
    }
    printf("Connected to %s:%d\n", config.host, config.port);
    // 创建SSH会话
    session = libssh2_session_init();
    if (!session)
    {
        fprintf(stderr, "Error creating SSH session\n");
        close(sock);
        libssh2_exit();
        return 1;
    }

    // 设置阻塞模式
    libssh2_session_set_blocking(session, 1);

    // 启动SSH握手
    rc = libssh2_session_handshake(session, sock);
    if (rc != 0)
    {
        fprintf(stderr, "Error establishing SSH session: %d\n", rc);
        goto cleanup;
    }

    if (config.verbose)
    {
        const char *fingerprint = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
        printf("Server fingerprint: ");
        for (int i = 0; i < 20; i++)
        {
            printf("%02X ", (unsigned char)fingerprint[i]);
        }
        printf("\n");
    }

    // 身份验证
    rc = libssh2_userauth_password(session, config.username, config.password);
    if (rc != 0)
    {
        fprintf(stderr, "Authentication failed: %d\n", rc);
        goto cleanup;
    }

    if (config.verbose)
    {
        printf("Authenticated successfully\n");
    }

    // 执行文件操作
    if (config.operation == 0)
    {
        rc = upload_file(&config, session);
    }
    else
    {
        rc = download_file(&config, session);
    }

cleanup:
    // 清理资源
    if (session)
    {
        libssh2_session_disconnect(session, "Normal shutdown");
        libssh2_session_free(session);
    }
    if (sock != -1)
    {
        close(sock);
    }
    libssh2_exit();

    // 释放配置内存
    free(config.host);
    free(config.username);
    free(config.password);
    free(config.local_path);
    free(config.remote_path);

    return rc == 0 ? 0 : 1;
}