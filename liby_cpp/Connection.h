#ifndef POLLERTEST_CONNECTION_H
#define POLLERTEST_CONNECTION_H

#include "Buffer.h"
#include "util.h"
#include <memory>

namespace Liby {
class Chanel;
class File;
class Poller;

class Connection final : clean_,
                         public std::enable_shared_from_this<Connection> {
public:
    using ConnCallback = std::function<void(std::shared_ptr<Connection> &&)>;
    Connection(Poller *poller, std::shared_ptr<File> &fp);

    void init();

    void send(const char *buf, off_t len);
    void send(Buffer &buf);
    void send(Buffer &&buf);
    void send(const char *buf);
    void send(const char c);
    Buffer &read();

    void setReadCallback(ConnCallback cb);
    void setWritCallback(ConnCallback cb);
    void setErroCallback(ConnCallback cb);

    Poller *getPoller() const;
    int getConnfd() const;
    void runEventHandler(BasicHandler handler);

    void suspendRead(bool flag = true);
    void suspendWrit(bool flag = true);

    void destroy();

private:
    void onRead();
    void onWrit();
    void onErro();

private:
    Poller *poller_;
    std::unique_ptr<Chanel> chan_;
    std::unique_ptr<Buffer> readBuf_;
    std::unique_ptr<Buffer> writBuf_;
    ConnCallback readEventCallback_;
    ConnCallback writeAllCallback_;
    ConnCallback errnoEventCallback_;
};
}

#endif // POLLERTEST_CONNECTION_H