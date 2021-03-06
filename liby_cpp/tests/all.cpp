#include "../Liby.h"
#include <map>

using namespace Liby;
using namespace std;

int main(void) {
    Logger::setLevel(Logger::LogLevel::DEBUG);
    EventLoop loop;

    auto chat_server = loop.creatTcpServer("localhost", "9379");
    chat_server->onAccept(
        [](std::shared_ptr<Connection> conn) { conn->suspendRead(false); });
    chat_server->onRead(
        [&chat_server](std::shared_ptr<Connection> conn) {
            Buffer buf(conn->read().retriveveAllAsString());
//            chat_server->runHanlderOnConns(
//                [buf, conn](std::shared_ptr<Connection> conn2) {
//                    if (conn != conn2) {
//                        conn2->send(buf.data(), buf.size());
//                    }
//                });
        });
    chat_server->start();

    auto daytime_server = loop.creatTcpServer("localhost", "9390");
    daytime_server->onAccept([](std::shared_ptr<Connection> conn) {
        conn->suspendRead();
        conn->send(Buffer(Timestamp::now().toString()));
        conn->send('\n');
    });
    daytime_server->onWritAll([&daytime_server](
        std::shared_ptr<Connection> conn) { conn->destroy(); });
    daytime_server->start();

    auto discard_server = loop.creatTcpServer("localhost", "9378");
    discard_server->onAccept(
        [](std::shared_ptr<Connection> conn) { conn->suspendRead(false); });
    discard_server->onRead([](std::shared_ptr<Connection> conn) {
        info(conn->read().retriveveAllAsString().c_str());
    });
    discard_server->start();

    auto echo_server = loop.creatTcpServer("localhost", "9377");
    map<Connection *, TimerId> timerIds;
    echo_server->onAccept(
        [&timerIds](std::shared_ptr<Connection> conn) {
            conn->suspendRead(false);
            conn->runEvery(Timestamp(2, 0),
                           [conn] { conn->send("Time is money\n"); });
        });
    echo_server->onRead([&timerIds](
        std::shared_ptr<Connection> conn) { conn->send(conn->read()); });
    ;
    echo_server->start();

    loop.RunMainLoop();
    return 0;
}
