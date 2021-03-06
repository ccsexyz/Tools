#ifdef __linux__
#include <sys/eventfd.h>
#endif
#include "Channel.h"
#include "EventQueue.h"
#include "FileDescriptor.h"
#include "Poller.h"
#include <string.h>
#include <unistd.h>

using namespace Liby;

EventQueue::EventQueue(Poller *poller) : poller_(poller) {
    fatalif(poller_ == nullptr, "Poller cannot be nullptr");
#ifdef __linux__
    eventfd_ = ::eventfd(1000, EFD_CLOEXEC | EFD_NONBLOCK);
    fatalif(eventfd_ < 0, "eventfd create: %s", ::strerror(errno));
    eventfp_ = std::make_shared<FileDescriptor>(eventfd_);
#elif defined(__APPLE__)
    int fds[2];
    if (::pipe(fds) < 0)
        fatal("pipe: %s", ::strerror(errno));
    eventfd_ = fds[0];
    event2fd_ = fds[1];
    eventfp_ = std::make_shared<FileDescriptor>(eventfd_);
    event2fp_ = std::make_shared<FileDescriptor>(event2fd_);
#endif
    eventChanelPtr_ = std::make_unique<Channel>(poller_, eventfp_);
    eventHandlersPtr_ = std::make_unique<BlockingQueue<BasicHandler>>();
}

EventQueue::~EventQueue() {
    //　使eventChanelPtr_析构时不去调用Poller基类的虚函数
    if (eventChanelPtr_) {
        eventChanelPtr_->setPoller(nullptr);
    }
}

void EventQueue::destroy() {
    eventfd_ = -1;
    eventfp_.reset();
#ifdef __APPLE__
    event2fd_ = -1;
    event2fp_.reset();
#endif
    eventChanelPtr_.reset();
    eventHandlersPtr_.reset();
}

void EventQueue::start() {
    eventChanelPtr_->enableRead();
    eventChanelPtr_->onErro([this] { destroy(); });
    eventChanelPtr_->onRead([this] {
        if (!eventfp_)
            return;

        char buf[128];
        int ret = ::read(eventfd_, buf, 128);
        if (ret <= 0)
            return;

        while (!eventHandlersPtr_->empty()) {
            auto f_ = eventHandlersPtr_->pop_front();
            f_();
        }
    });
    eventChanelPtr_->addChanel();
}

void EventQueue::wakeup() {
    static int64_t this_is_a_number = 1;
#ifdef __linux__
    ::write(eventfd_, &this_is_a_number, sizeof(this_is_a_number));
#elif defined(__APPLE__)
    ::write(event2fd_, &this_is_a_number, sizeof(this_is_a_number));
#endif
}

void EventQueue::pushHandler(const BasicHandler &handler) {
    eventHandlersPtr_->push_back(handler);
}

void EventQueue::setPoller(Poller *poller) {
    poller_ = poller;
    eventChanelPtr_->setPoller(poller_);
}
