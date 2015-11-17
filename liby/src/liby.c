#include "liby.h"

liby_server *
liby_server_init(const char *server_path, const char *server_port)
{
    if(server_path == NULL || server_port == NULL) {
        fprintf(stderr, "server name or port must be valid!\n");
        return NULL;
    }

    char *path = strdup(server_path); //must be free
    char *port = strdup(server_port);

    liby_server *server = safe_malloc(sizeof(liby_server));
    memset((void *)server, 0, sizeof(liby_server));

    server->server_path = path;
    server->server_port = port;
    server->type = 1;
    server->listenfd = initserver(path, port);
    set_noblock(server->listenfd);
    //printf("listenfd = %d\n", server->listenfd);

    return server;
}

void
liby_server_destroy(liby_server *server)
{
    if(server) {
        if(server->server_path) free(server->server_path);
        if(server->server_port) free(server->server_port);
        close(server->listenfd);

        if(server->buf_allocate_flag && server->buf) {
            free(server->buf);
        }

        liby_client *p = server->head;
        while(p) {
            liby_client *next = p->next;
            liby_client_release(p);
            p = next;
        }

        free(server);
    }
}

void
add_server_to_epoller(liby_server *server, epoller_t *loop)
{
    if(server == NULL || loop == NULL) {
        fprintf(stderr, "server or epoller must be valid!\n");
        return;
    }

    server->loop = loop;

    struct epoll_event *event = &(loop->event);
    event->data.ptr = (void *)server;
    event->events = EPOLLIN | EPOLLHUP | EPOLLET;
    epoll_add(loop, server->listenfd);

    if(loop->running_servers == 0)
        loop->flag = !0;
    loop->running_servers++;
}

void
add_client_to_epoller(liby_client *client, epoller_t *loop)
{
    if(client == NULL || loop == NULL) {
        fprintf(stderr, "%s\n", "client or epoller must be valid!\n");
        return;
    }

    struct epoll_event *event = client->event;
    event->data.ptr = (void *)client;
    event->events = EPOLLIN | EPOLLHUP | EPOLLET;
    epoll_add1(loop, client->sockfd, event);
}

void
add_client_to_epoller1(liby_client *client, epoller_t *loop)
{
    if(client == NULL || loop == NULL) {
        fprintf(stderr, "%s\n", "client or epoller must be valid!\n");
        return;
    }

    struct epoll_event *event = client->event;
    event->data.ptr = (void *)client;
    event->events = EPOLLOUT | EPOLLHUP | EPOLLET;
    epoll_add1(loop, client->sockfd, event);
}

void
handle_epoll_event(epoller_t *loop, int n)
{
    for(int i = 0; loop->flag && (i < n); i++) {
        struct epoll_event *p = &(loop->events[i]);

        liby_server *server = (liby_server *)(p->data.ptr);
        if(server == NULL) {
            fprintf(stderr, "pointer is NULL!!!\n");
            //getchar();
            continue;
        }

        if(server->type) {
            server->event = p;
            epoll_acceptor(server);
            continue;
        } else {
            //printf("client!\n");
        }

        liby_client *client = (liby_client *)(p->data.ptr);
        if(p->events & EPOLLHUP) {
            del_client_from_server(client, client->server);
            continue;
        } else if(p->events & EPOLLIN) {
            //printf("some message in!\n");
            read_message(client);
        } else if(p->events & EPOLLOUT) {
            //printf("some message out!\n");
            write_message(client);
        }
    }
}

void
epoll_acceptor(liby_server *server)
{
    if(server == NULL) return;
    int listen_fd = server->listenfd;

    while(1) {
        int clfd = accept(listen_fd, NULL, NULL);
        if(clfd < 0) {
            if(errno == EAGAIN) {
                break;
            } else {
                fprintf(stderr, "accept error: %s\n", strerror(errno));
                break;
            }
        }

        set_noblock(clfd);

        liby_client *client = liby_client_init_by_server(clfd, server);
        add_client_to_server(client, server);
        add_client_to_epoller(client, server->loop);

        if(server->acceptor) {
            server->acceptor((void *)client);
        } else {
            printf("no acceptor!\n");
        }
    }
}

liby_client *
liby_client_init_by_server(int fd, liby_server *server)
{
    if(server == NULL) {
        close(fd);
        return NULL;
    }

    liby_client *c = (liby_client *)safe_malloc(sizeof(liby_client));
    memset((void *)c, 0, sizeof(liby_client));
    //c->type = 0;
    c->sockfd = fd;
    c->server = server;
    c->is_created_by_server = 1;
    c->loop = server->loop;
    c->event = &(c->event_);
    //c->event->data.ptr = c;

    add_client_to_epoller(c, c->loop);

    return c;
}

liby_client *
liby_client_init(int fd, epoller_t *loop)
{
    if(loop == NULL) {
        close(fd);
        return NULL;
    } else {
        liby_client *c = (liby_client *)safe_malloc(sizeof(liby_client));
        memset((void *)c, 0, sizeof(liby_client));
        c->sockfd = fd;
        c->loop = loop;
        c->event = &(c->event_);
        //c->event->data.ptr = c;
        add_client_to_epoller1(c, loop);

        return c;
    }
}

int
liby_sync_connect_tcp(const char *host, const char *port)
{
    return connect_tcp(host, port);
}

void
liby_client_release(liby_client *c)
{
    if(c == NULL) {
        return;
    }

    liby_client_release_data(c);
    close(c->sockfd);
    free(c);
}

void
add_client_to_server(liby_client *client, liby_server *server)
{
    if(client == NULL || server == NULL) {
        return;
    }

    liby_client *p = server->tail;
    if(p == NULL) {
        server->head = server->tail = client;
        client->next = NULL;
        client->prev = NULL;
    } else {
        server->tail->next = client;
        client->prev = server->tail;
        server->tail = client;
        client->next = NULL;
    }
}

void
del_client_from_epoller(liby_client *client, epoller_t *loop)
{
    //printf("dleete one!!!\n");
    liby_client_release(client);
}

void
del_client_from_server(liby_client *client, liby_server *server)
{
    if(client == NULL || server == NULL) {
        return;
    }

    liby_client *p = server->head;
    while(p) {
        if(p == client) {
            if(p->prev != NULL) {
                p->prev->next = p->next;
            }
            if(p->next != NULL) {
                p->next->prev = p->prev;
            }
            if(p == server->head) {
                server->head = p->next;
            }
            if(p == server->tail) {
                server->tail = p->prev;
            }

            liby_client_release(client);

            return;
        } else {
            p = p->next;
        }
    }

    //fprintf(stderr, "no this client with pointer %p\n", client);
}

static int
liby_client_readable(liby_client *client)
{
    if(client) {
        if(client->read_list_head || client->curr_read_task)
            return 1;
        else
            return 0;
    } else {
        return 0;
    }
}

static int
liby_client_writeable(liby_client *client)
{
    if(client) {
        if(client->write_list_head || client->curr_write_task)
            return 1;
        else
            return 0;
    } else {
        return 0;
    }
}

void
read_message(liby_client *client)
{
    if(client == NULL) return;
    liby_server *server = client->server;
    if(server == NULL && client->is_created_by_server) {
        liby_client_release(client);
        return;
    }
    epoller_t *loop = client->loop;

    if(client->curr_read_task == NULL) {
        //printf("curr_read_task = NULL!\n");
        client->curr_read_task = pop_io_task_from_client(client, 1);
        if(client->curr_read_task == NULL) {
            //printf("no read task!\n");
            return;
        }
    }

    while(client->curr_read_task) {
        io_task *p = client->curr_read_task;

        while(1) {
            int ret = read(client->sockfd, p->buf + p->offset, p->size - p->offset);
            if(ret > 0) {
                p->offset += ret;
                if(p->offset >= p->min_except_bytes) {
                    //struct epoll_event *event = &(loop->event);
                    //event->data.ptr = (void *)client;
                    //event->events = EPOLLHUP | EPOLLET;
                    //epoll_mod(loop, client->sockfd);

                    if(p->handler) {
                        p->handler(client, p->buf, p->offset, 0);
                    }
                    if(server && server->read_complete_handler) {
                        server->read_complete_handler(client, p->buf, p->offset, 0);
                    }
                    if(client->read_complete_handler) {
                        client->read_complete_handler(client, p->buf, p->offset, 0);
                    }
                    break;
                }
            } else {
                if(ret != 0 && errno == EAGAIN) {
                    if(!liby_client_readable(client))
                        disable_epollin(client);
                    return;
                } else {
                    if(p->handler) {
                        p->handler(client, p->buf, p->offset, errno);
                    }
                    if(server && server->read_complete_handler) {
                        server->read_complete_handler(client, p->buf, p->offset, errno);
                    }
                    if(client->read_complete_handler) {
                        client->read_complete_handler(client, p->buf, p->offset, errno);
                    }

                    if(client->is_created_by_server)
                        del_client_from_server(client, server);
                    else
                        del_client_from_epoller(client, loop);

                    return;
                }
            }
        }

        client->curr_read_task = pop_io_task_from_client(client, 1);
        free(p);
    }
}

void
write_message(liby_client *client)
{
    if(client == NULL) return;
    liby_server *server = client->server;
    if(server == NULL && client->is_created_by_server) {
        liby_client_release(client);
        return;
    }
    epoller_t *loop = client->loop;

    if(client->curr_write_task == NULL) {
        client->curr_write_task = pop_io_task_from_client(client, 0);
        if(client->curr_write_task == NULL) {
            disable_epollout(client);
            if(!client->is_created_by_server && client->conn_func) {
                client->conn_func(client);
            }
            return;
        }
    }

    while(client->curr_write_task) {
        io_task *p = client->curr_write_task;

        while(1) {
            //printf("try to write!\n");
            int ret = write(client->sockfd, p->buf + p->offset, p->size - p->offset);
            if(ret > 0) {
                p->offset += ret;
                if(p->offset >= p->min_except_bytes) {
                    //struct epoll_event *event = &(loop->event);
                    //event->data.ptr = (void *)client;
                    //event->events = EPOLLHUP | EPOLLET;
                    //epoll_mod(loop, client->sockfd);

                    if(p->handler) {
                        p->handler(client, p->buf, p->offset, 0);
                    }
                    if(server && server->write_complete_handler) {
                        server->write_complete_handler(client, p->buf, p->offset, 0);
                    }
                    if(client->write_complete_handler) {
                        client->write_complete_handler(client, p->buf, p->offset, 0);
                    }
                    break;
                }
            } else {
                if(ret != 0 && errno == EAGAIN) {
                    if(liby_client_writeable(client))
                        disable_epollout(client);
                    return;
                } else {
                    if(p->handler) {
                        p->handler(client, p->buf, p->offset, errno);
                    }
                    if(server && server->write_complete_handler) {
                        server->write_complete_handler(client, p->buf, p->offset, errno);
                    }
                    if(client->write_complete_handler) {
                        client->write_complete_handler(client, p->buf, p->offset, errno);
                    }

                    if(client->is_created_by_server)
                        del_client_from_server(client, server);
                    else
                        del_client_from_epoller(client, loop);

                    return;
                }
            }
        }

        client->curr_write_task = pop_io_task_from_client(client, 0);
        free(p);
    }
}

void
push_io_task_to_client(io_task *task, liby_client *client, int type) //type == 1 read type == 0 write
{
    if(task == NULL || client == NULL) {
        //printf("task or client is NULL!\n");
        return;
    }

    if(type) {
        LOCK(client->io_read_mutex);
        push_io_task(&(client->read_list_head), &(client->read_list_tail), task);
        UNLOCK(client->io_read_mutex);
    } else {
        LOCK(client->io_write_mutex);
        push_io_task(&(client->write_list_head), &(client->write_list_tail), task);
        UNLOCK(client->io_write_mutex);
    }
}

void
push_io_task(io_task **head, io_task **tail, io_task *p)
{
    if(*head == NULL) {
        //printf("head is NULL!!!\n");
        *head = *tail = p;
        p->next = p->prev = NULL;
    } else {
        p->prev = *tail;
        (*tail)->next = p;
        (*tail) = p;
    }
}

io_task *
pop_io_task(io_task **head, io_task **tail)
{
    if(*head == NULL) {
        return NULL;
    } else {
        io_task *p = *head;
        *head = p->next;

        if(*head == NULL) {
            *tail = NULL;
        }

        return p;
    }
}

io_task *
pop_io_task_from_client(liby_client *client, int type) // 1 for read, 0 for write
{
    io_task *ret = NULL;

    if(client == NULL) {
        //printf("client is NULL!\n");
        return NULL;
    }

    if(type == 1) {
        LOCK(client->io_read_mutex);
        ret = pop_io_task(&(client->read_list_head), &(client->read_list_tail));
        UNLOCK(client->io_read_mutex);
    } else {
        LOCK(client->io_write_mutex);
        ret = pop_io_task(&(client->write_list_head), &(client->write_list_tail));
        UNLOCK(client->io_write_mutex);
    }

    return ret;
}

void
liby_async_read_some(liby_client *client, char *buf, off_t buffersize, handle_func handler)
{
    if(client == NULL) return;

    if(buf == NULL) {
        buf = safe_malloc(DEFAULT_BUFFER_SIZE);
        buffersize = DEFAULT_BUFFER_SIZE;
    }

    io_task *task = (io_task *)safe_malloc(sizeof(io_task));
    task->buf = buf;
    task->size = buffersize;
    task->offset = 0;
    task->min_except_bytes = 0;
    task->handler = handler;

    push_io_task_to_client(task, client, 1);
    //printf("push one io_task!\n");

    //liby_server *server = client->server;
    /*
    epoller_t *loop = client->loop;
    struct epoll_event *event = &(loop->event);
    event->data.ptr = (void *)client;
    event->events = EPOLLIN | EPOLLHUP | EPOLLET;
    epoll_mod(loop, client->sockfd);
    */

    enable_epollin(client);
}

void
liby_async_read(liby_client *client, handle_func handler)
{
    liby_async_read_some(client, NULL, 0, handler);
}

void
liby_async_write_some(liby_client *client, char *buf, off_t buffersize, handle_func handler)
{
    if(client == NULL) return;
    if(buf == NULL || buffersize == 0) return;

    io_task *task = (io_task *)safe_malloc(sizeof(io_task));
    task->buf = buf;
    task->size = buffersize;
    task->offset = 0;
    task->min_except_bytes = 0;
    task->handler = handler;

    push_io_task_to_client(task, client, 0);

    //liby_server *server = client->server;
    /*
    epoller_t *loop = client->loop;
    struct epoll_event *event = &(loop->event);
    event->data.ptr = (void *)client;
    event->events = EPOLLOUT | EPOLLHUP | EPOLLET;
    epoll_mod(loop, client->sockfd);
    */

    enable_epollout(client);
}

void
set_write_complete_handler_for_server(liby_server *server, handle_func handler)
{
    if(server != NULL) {
        server->write_complete_handler = handler;
    }
}

void
set_read_complete_handler_for_server(liby_server *server, handle_func handler)
{
    if(server != NULL) {
        server->read_complete_handler = handler;
    }
}

void
set_acceptor_for_server(liby_server *server, accept_func acceptor)
{
    if(server != NULL) {
        server->acceptor = acceptor;
    }
}

void
set_buffer_for_server(liby_server *server, char *buf, off_t buffersize)
{
    if(buffersize == 0) {
        buffersize = DEFAULT_BUFFER_SIZE;
    }

    if(buf == NULL) {
        server->buf_allocate_flag = 1;
        server->buf = (char *)safe_malloc(buffersize);
    } else {
        server->buf = buf;
    }
}

void
set_default_buffer_for_server(liby_server *server)
{
    set_buffer_for_server(server, NULL, 0);
}

char *
get_server_buffer(liby_server *server)
{
    if(server == NULL) {
        return NULL;
    } else {
        return server->buf;
    }
}

off_t
get_server_buffersize(liby_server *server)
{
    if(server == NULL) {
        return 0;
    } else {
        return server->buffersize;
    }
}

void
set_buffer_for_client(liby_client *client, char *buf, off_t buffersize)
{
    if(buffersize == 0) {
        buffersize = DEFAULT_BUFFER_SIZE;
    }

    if(buf == NULL) {
        client->buf_allocate_flag = 1;
        client->buf = (char *)safe_malloc(buffersize);
    } else {
        client->buf = buf;
    }
}

void
set_default_buffer_for_client(liby_client *client)
{
    set_buffer_for_client(client, NULL, 0);
}

char *
get_client_buffer(liby_client *client)
{
    if(client == NULL) {
        return NULL;
    } else {
        return client->buf;
    }
}

off_t
get_client_buffersize(liby_client *client)
{
    if(client == NULL) {
        return 0;
    } else {
        return client->buffersize;
    }
}

epoll_event_handler
get_default_epoll_handler(void)
{
    return handle_epoll_event;
}

void
set_data_of_client(liby_client *client, void *data, release_func func)
{
    if(client) {
        if(client->data) {
            liby_client_release_data(client);
        }

        client->data = data;
        client->data_release_func = func;
    }
}

void
liby_client_release_data(liby_client *client)
{
    if(client) {
        if(client->data_release_func) {
            client->data_release_func(client->data);
            client->data_release_func = NULL;
        } else {
            free(client->data);
        }

        client->data = NULL;
    }
}

static void
non_free_release(void *data)
{
    ;
}

void
set_data_of_client_with_free(liby_client *client, void *data)
{
    set_data_of_client(client, data, NULL);
}

void
set_data_of_client_without_free(liby_client *client, void *data)
{
    set_data_of_client(client, data, non_free_release);
}

void *
get_data_of_client(liby_client *client)
{
    if(client) {
        return client->data;
    } else {
        return NULL;
    }
}

void
set_connect_handler_for_client(liby_client *client, connect_func conn_func)
{
    if(client) {
        client->conn_func = conn_func;
    }
}

void
set_read_complete_handler_for_client(liby_client *client, handle_func handler)
{
    if(client) {
        client->read_complete_handler = handler;
    }
}

void
set_write_complete_handler_for_client(liby_client *client, handle_func handler)
{
    if(client) {
        client->write_complete_handler = handler;
    }
}

static void
liby_epoll_common_func(void *client_or_server, struct epoll_event **event_, epoller_t **loop_, int *fd_)
{
    if(event_ && loop_ && fd_ && client_or_server) {
        liby_server *server = (liby_server *)client_or_server;
        if(server->type) {
            *event_ = server->event;
            *loop_ = server->loop;
            *fd_ = server->listenfd;
        } else {
            liby_client *client = (liby_client *)client_or_server;
            *event_ = client->event;
            *loop_ = client->loop;
            *fd_ = client->sockfd;
        }
        //(*event_)->data.ptr = client_or_server;
    }
}

void
enable_epollin(void *client_or_server)
{
    struct epoll_event *event;
    epoller_t *loop;
    int fd;

    liby_epoll_common_func(client_or_server, &event, &loop, &fd);

    event->events |= (EPOLLIN);

    epoll_mod1(loop, fd, event);
}

void
disable_epollin(void *client_or_server)
{
    struct epoll_event *event;
    epoller_t *loop;
    int fd;

    liby_epoll_common_func(client_or_server, &event, &loop, &fd);

    event->events &= ~(EPOLLIN);

    epoll_mod1(loop, fd, event);
}

void
enable_epollout(void *client_or_server)
{
    struct epoll_event *event;
    epoller_t *loop;
    int fd;

    liby_epoll_common_func(client_or_server, &event, &loop, &fd);

    event->events |= (EPOLLOUT);

    epoll_mod1(loop, fd, event);
}

void
disable_epollout(void *client_or_server)
{
    struct epoll_event *event;
    epoller_t *loop;
    int fd;

    liby_epoll_common_func(client_or_server, &event, &loop, &fd);

    event->events &= ~(EPOLLOUT);

    epoll_mod1(loop, fd, event);
}
