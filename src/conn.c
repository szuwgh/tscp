#include "conn.h"

static void buf_clear(struct buf *buf)
{
    // No capacity means this buffer is owned somewhere else and we
    // must not free the data.
    if (buf->cap)
    {
        free(buf->data);
    }
    memset(buf, 0, sizeof(struct buf));
}

void ev_process(struct net_conn *conn, const void *p, size_t len, void *udata)

{
    printf("Received data from fd %d: %.*s\n", conn->fd, (int)len, (char *)p);
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