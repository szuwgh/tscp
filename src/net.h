#ifndef NET_H
#define NET_H
#include <stdbool.h>
#include <stdio.h>
#include "hash.h"

typedef struct epoll_event event_t;

struct tscontext
{
    int epfd;      // epoll 实例 fd
    int index;     // 线程编号
    int listen_fd; // 监听 socket fd
    int queuesize; // 当前连接数
    int nevents;   // epoll_wait 返回的事件数
    int nthreads;  // 工作线程数
    // int nqreads;   // 读队列长度

    // 各队列的当前长度计数器
    int nreadsq;   // 待读取连接数
    int ninsq;     // 待处理连接数 对应ins_que队列长度
    int nclosesq;  // 待关闭连接数
    int nattachsq; // 待附加连接数
    int noutsq;    // 待写入连接数

    event_t *events; // 事件数组

    struct net_conn **pread_que;   // 待读读队列
    struct net_conn **ins_que;     // 已读取数据待处理的连接队列
    struct net_conn **attachs_que; // 后台工作完成后待重新附加的连接队列
    struct net_conn **outs_que;    // 待发送数据的连接队列
    struct net_conn **closes_que;  // 待关闭连接队列
    void *udata;                   // 用户数据指针 由用户在opened回调中设置 后续事件处理函数可通过连接上下文访问
    char *inpkts;                  // 数据包指针数组
    // 数据包元数据
    char **inspkts_que;  // 对应ins_que队列中各连接的数据包指针数组
    int *inspktlens_que; // 对应ins_que队列中各连接的数据包长度数组

    void (*process)(struct net_conn *, const void *, size_t, void *); // 数据到达时的处理函数 刚读出来的字节缓冲地址；这是 qread 队列里为每个连接准备的 packet 缓冲，回调直接解析这批数据，无需再拷贝
                                                                      // 本次缓冲的有效长度，用来告诉回调应该消费多少字节
    void (*opened)(struct net_conn *, void *);                        // 新连接建立时的回调
    void (*closed)(struct net_conn *, void *);                        // 连接关闭时的回调

    struct tscontext *ctxs; // 连接上下文数组
    hmap hmap;
};

struct net_conn
{
    int fd;
    bool closed;
    void *udata;
    char *out;
    size_t outlen;
    size_t outcap;
    void *bgctx; // 后台工作上下文指针
    struct tscontext *ctx;
};

int net_listen(int nthread);

void net_conn_setudata(struct net_conn *conn, void *udata);
void *net_conn_udata(struct net_conn *conn);

int add_write(int qfd, int fd);
#endif