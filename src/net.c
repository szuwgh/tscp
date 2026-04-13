
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <stdatomic.h>
#include "net.h"
#include "conn.h"

#define PACKETSIZE 16384 // 16KB

static int setnonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

inline static int add_read(int epfd, int fd)
{
    event_t ev;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev); // 线程安全
}

int add_write(int qfd, int fd)
{

    struct epoll_event ev = {0};
    ev.events = EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl(qfd, EPOLL_CTL_ADD, fd, &ev);
}

static int get_event_fd(event_t *ev)
{
    return ev->data.fd;
}

static struct net_conn *conn_new(int fd, struct tscontext *ctx)
{
    struct net_conn *conn = malloc(sizeof(struct net_conn));
    memset(conn, 0, sizeof(struct net_conn));
    conn->fd = fd;
    conn->ctx = ctx;
    return conn;
}

void net_conn_setudata(struct net_conn *conn, void *udata)
{
    conn->udata = udata;
}

void *net_conn_udata(struct net_conn *conn)
{
    return conn->udata;
}

inline static void ts_accept(struct tscontext *sctx)
{
    static atomic_uint_fast64_t next_ctx_index = 0;
    // Accept new connections
    for (int i = 0; i < sctx->nevents; i++)
    {
        int fd = get_event_fd(&sctx->events[i]);
        struct net_conn **conn_slot = hmap_get_value(&sctx->hmap, &fd);
        struct net_conn *conn = conn_slot ? *conn_slot : NULL;
        if (!conn)
        {
            if (fd == sctx->listen_fd)
            {

                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int conn_fd = accept(sctx->listen_fd, (struct sockaddr *)&client_addr, &client_len);
                if (conn_fd < 0)
                {
                    continue;
                }
                // Set non-blocking
                setnonblock(conn_fd);
                int idx = atomic_fetch_add(&next_ctx_index, 1) % sctx->nthreads;
                // Add to epoll
                if (add_read(sctx->ctxs[idx].epfd, conn_fd) < 0)
                {
                    perror("epoll_ctl ADD failed");
                    close(conn_fd);
                }
                continue;
                printf("Accepted new connection: fd %d\n", conn_fd);
            }
            // create new conn
            conn = conn_new(fd, sctx);
            hmap_insert(&sctx->hmap, &fd, &conn);
            sctx->opened(conn, sctx->udata);
        }

        if (conn->bgctx)
        {
            // BGWORK(2)
            // The connection has been added back to the event loop, but it
            // needs to be attached and restated.
            sctx->attachs_que[sctx->nattachsq++] = conn;
        }
        else if (conn->outlen > 0)
        {
            sctx->outs_que[sctx->noutsq++] = conn;
        }
        else if (conn->closed)
        {
            sctx->closes_que[sctx->nclosesq++] = conn;
        }
        else
        {
            sctx->pread_que[sctx->nreadsq++] = conn;
        }
    }
}

inline static void handle_read(ssize_t n, char *pkt, struct net_conn *conn, struct tscontext *sctx)
{
    if (n <= 0)
    {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            return;
        sctx->closes_que[sctx->nclosesq++] = conn;
        return;
    }

    pkt[n] = '\0';
    sctx->ins_que[sctx->ninsq] = conn;
    sctx->inspkts_que[sctx->ninsq] = pkt;
    sctx->inspktlens_que[sctx->ninsq] = n;
    sctx->ninsq++;
}

// 处理待读取事件
inline static void ts_read(struct tscontext *sctx)
{
    for (int i = 0; i < sctx->nreadsq; i++)
    {
        struct net_conn *conn = sctx->pread_que[i];
        char *pkt = sctx->inpkts + (i * PACKETSIZE);
        ssize_t n = read(conn->fd, pkt, PACKETSIZE - 1);
        handle_read(n, pkt, conn, sctx);
    }
}

inline static void flush_conn(struct net_conn *conn, size_t written)
{
    while (written < conn->outlen)
    {
        ssize_t n;
        n = write(conn->fd, conn->out + written, conn->outlen - written);
        if (n == -1)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            conn->closed = true;
            break;
        }
        written += n;
    }
    // either everything was written or the socket is closed
    conn->outlen = 0;
}

inline static void ts_write(struct tscontext *sctx)
{
    for (int i = 0; i < sctx->noutsq; i++)
    {
        struct net_conn *conn = sctx->outs_que[i];
        flush_conn(conn, 0);
        if (conn->closed)
        {
            sctx->closes_que[sctx->nclosesq++] = conn;
        }
    }
}

static void conn_free(struct net_conn *conn)
{
    if (conn)
    {
        if (conn->out)
        {
            free(conn->out);
        }
        free(conn);
    }
}

inline static void ts_close(struct tscontext *sctx)
{
    // Close all sockets that need to be closed
    for (int i = 0; i < sctx->nclosesq; i++)
    {
        struct net_conn *conn = sctx->closes_que[i];
        sctx->closed(conn, sctx->udata);
        close(conn->fd);

        hmap_delete(&sctx->hmap, &conn->fd, NULL);
        // atomic_fetch_sub_explicit(&nconns, 1, __ATOMIC_RELEASE);
        // atomic_fetch_sub_explicit(&ctx->nconns, 1, __ATOMIC_RELEASE);
        conn_free(conn);
    }
}

// 处理业务逻辑
inline static void ts_process(struct tscontext *sctx)
{
    for (int i = 0; i < sctx->ninsq; i++)
    {
        struct net_conn *conn = sctx->ins_que[i];
        char *p = sctx->inspkts_que[i];
        int n = sctx->inspktlens_que[i];
        // 调用用户定义的数据处理函数 实际业务逻辑在这里执行
        sctx->process(conn, p, n, NULL);
        if (conn->bgctx)
        {
            // BGWORK(1)
            // Connection entered background mode.
            // This means the connection is no longer in the event queue but
            // is still owned by this qthread. Once the bgwork is done the
            // connection will be added back to the queue with addwrite.
        }
        else if (conn->outlen > 0)
        {
            // 阻塞发送
            sctx->outs_que[sctx->noutsq++] = conn;
        }
        else if (conn->closed)
        {
            sctx->closes_que[sctx->nclosesq++] = conn;
        }
    }
}

inline static void ts_reset(struct tscontext *ctx)
{
    ctx->nreadsq = 0;
    ctx->ninsq = 0;
    ctx->nclosesq = 0;
    ctx->noutsq = 0;
    ctx->nattachsq = 0;
}

inline static void ts_attach(struct tscontext *ctx)
{
}

static void *ts_worker_loop(void *arg)
{
    struct tscontext *sc = (struct tscontext *)arg;
    sc->events = malloc(sizeof(event_t) * sc->queuesize);

    printf("Worker thread %d started with epfd %d\n", sc->index, sc->epfd);
    while (1)
    {
        int n = epoll_wait(sc->epfd, sc->events, sc->queuesize, -1);
        if (n < 0)
        {
            perror("epoll_wait failed");
            continue;
        }
        sc->nevents = n;
        ts_reset(sc);
        ts_attach(sc);
        ts_accept(sc);
        ts_read(sc);
        ts_process(sc);
        ts_write(sc);
        ts_close(sc);
    }
    return NULL;
}

// create_worker_thread 函数
int create_worker_thread(struct tscontext *wk, int index)
{
    // 1. 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll_create1 failed");
        return -1;
    }
    wk->epfd = epfd;
    wk->index = index;
    add_read(epfd, wk->listen_fd);
    // 2. 创建线程
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, ts_worker_loop, wk);
    if (ret != 0)
    {
        perror("pthread_create failed");
        close(epfd);
        return -1;
    }
    return epfd;
}

int create_worker(struct tscontext *wk, int index)
{
    // 1. 创建 epoll 实例
    int epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll_create1 failed");
        return -1;
    }
    wk->epfd = epfd;
    wk->index = index;
    add_read(epfd, wk->listen_fd);
    ts_worker_loop(wk);
    return epfd;
}

static int listen_tcp(const char *host, const char *port)
{
    if (!port || !*port || strcmp(port, "0") == 0)
    {
        return 0;
    }
    struct addrinfo hints = {0};
    struct addrinfo *addrs;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int ret = getaddrinfo(host, port, &hints, &addrs);
    if (ret != 0)
    {
        fprintf(stderr, "getaddrinfo: %s: %s:%s", gai_strerror(ret), host,
                port);
        abort();
    }
    struct addrinfo *ainfo = addrs;
    assert(addrs != NULL);
    while (ainfo->ai_family != PF_INET)
    {
        ainfo = ainfo->ai_next;
    }
    assert(ainfo);
    int fd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (fd == -1)
    {
        perror("# socket(tcp)");
        abort();
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (ret == -1)
    {
        perror("# setsockopt(reuseaddr)");
        abort();
    }
    ret = setnonblock(fd);
    if (ret == -1)
    {
        perror("# setnonblock");
        abort();
    }
    ret = bind(fd, ainfo->ai_addr, ainfo->ai_addrlen);
    if (ret == -1)
    {
        fprintf(stderr, "# bind(tcp): %s:%s", host, port);
        abort();
    }
    ret = listen(fd, 128);
    if (ret == -1)
    {
        fprintf(stderr, "# listen(tcp): %s:%s", host, port);
        abort();
    }
    freeaddrinfo(addrs);
    return fd;
}

void free_tscontext(struct tscontext *ctx)
{
    if (!ctx)
        return;
    free(ctx->events);
    free(ctx->pread_que);
    free(ctx->ins_que);
    free(ctx->outs_que);
    free(ctx->closes_que);
    free(ctx->attachs_que);
    free(ctx->inpkts);
    free(ctx->inspkts_que);
    free(ctx->inspktlens_que);
    hmap_deinit(&ctx->hmap);
}

int net_listen(int nthread)
{
    int fd = listen_tcp("127.0.0.1", "4567");
    struct tscontext *sc_list = malloc(sizeof(struct tscontext) * nthread);
    memset(sc_list, 0, sizeof(struct tscontext) * nthread);
    for (int i = 0; i < nthread; i++)
    {
        struct tscontext *sctx = &sc_list[i];
        sctx->listen_fd = fd;
        sctx->queuesize = 128;
        sctx->nthreads = nthread;
        sctx->ctxs = sc_list;
        sctx->pread_que = malloc(sizeof(struct net_conn *) * sctx->queuesize);
        sctx->ins_que = malloc(sizeof(struct net_conn *) * sctx->queuesize);
        sctx->outs_que = malloc(sizeof(struct net_conn *) * sctx->queuesize);
        sctx->closes_que = malloc(sizeof(struct net_conn *) * sctx->queuesize);
        sctx->attachs_que = malloc(sizeof(struct net_conn *) * sctx->queuesize);

        sctx->inpkts = malloc(PACKETSIZE * sctx->queuesize);
        sctx->inspkts_que = malloc(sizeof(char *) * sctx->queuesize);
        sctx->inspktlens_que = malloc(sizeof(int) * sctx->queuesize);
        hmap_init_int(&sctx->hmap, sizeof(struct net_conn *));
        sctx->process = ev_process;
        sctx->opened = ev_opened;
        sctx->closed = ev_closed;

        if (!sctx)
        {
            perror("malloc failed");
            return -1;
        }
        if (i == nthread - 1)
        {
            // 主线程也作为工作线程 这里会阻塞
            create_worker(sctx, i);
        }
        else
        {
            // 创建独立工作线程
            int epfd = create_worker_thread(sctx, i);
            if (epfd < 0)
            {
                return -1;
            }
            printf("Created worker thread %d with epoll fd %d\n", i, epfd);
        }
    }
    /* TODO: 在 shutdown 时统一释放 sc_list 及其队列/线程资源 */
    for (int i = 0; i < nthread; i++)
    {
        struct tscontext *sctx = &sc_list[i];
        free_tscontext(sctx);
    }
    free(sc_list);

    return 0;
}