##Liby

###基本用法

```c
#include "liby.h"

void read_all_handler(liby_client *client, char *buf, off_t length, int ec);

void write_all_handler(liby_client *client, char *buf, off_t length, int ec) {
    free(buf);
    if (ec == 0)
        liby_async_read(client, read_all_handler);
    }
}

void read_all_handler(liby_client *client, char *buf, off_t length, int ec) {
    if (ec == 0) {
        liby_async_write_some(client, buf, length, write_all_handler);
    }
}

void echo_aceptor(liby_client *client) {
    if (client == NULL)
        printf("client is NULL");
    liby_async_read(client, read_all_handler);
}

int main(int argc, char **argv) {
    liby_server *echo_server =
        liby_server_init("0.0.0.0", "9377", echo_aceptor);
    liby_loop *loop = liby_loop_create(8);
    add_server_to_loop(echo_server, loop);
    run_liby_main_loop(loop);
    return 0;
}
```
