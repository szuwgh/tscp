#include "conn.h"

static void buf_clear(struct buf *buf)
{
    if (buf->cap)
    {
        free(buf->data);
    }
    memset(buf, 0, sizeof(struct buf));
}

void ev_process(struct net_conn *conn, const void *p, size_t len, void *udata)

{
    printf("Received data from fd %d: %.*s\n", conn->fd, (int)len, (char *)p);
    struct Conn *c = net_conn_udata(conn);
    // 写一个Ok回去
    const char *response = "Ok\n";
    size_t resp_len = strlen(response);
    char *newbuf = realloc(conn->out, resp_len);
    if (!newbuf)
    {
        perror("realloc");
        conn->closed = true;
        return;
    }
    conn->out = newbuf;
    memcpy(conn->out, response, resp_len);
    conn->outlen = resp_len;
    // add_write(conn->ctx->epfd, conn->fd);
}

void ev_opened(struct net_conn *conn5, void *udata)
{
    struct Conn *conn = malloc(sizeof(struct Conn));
    memset(conn, 0, sizeof(struct Conn));
    conn->conn5 = conn5;
    net_conn_setudata(conn5, conn);
}

void ev_closed(struct net_conn *conn5, void *udata)
{
    (void)udata;
    struct Conn *conn = net_conn_udata(conn5);
    buf_clear(&conn->packet);
    // args_free(&conn->args);
    // pg_free(conn->pg);
    free(conn);
}
