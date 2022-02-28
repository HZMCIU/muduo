// Copyright 2010, Shuo Chen.  All rights reserved.  // http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Channel.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
/**
 * @comment poll macros
 * @macros
 * - POLLIN: There is data to read.
 * - POLLPRI: There is some exceptional condition on the file descriptor.  Possibilities include:
 *   - There is out-of-band data on a TCP socket
 *   - A pseudoterminal master in packet mode has seen a state change on the slave
 *   - A cgroup.events file has been modified
 * - POLLOUT: Writing is now possible, though a write larger than the available space in a socket or pipe will still block (unless O_NONBLOCK is set).
 */
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
    : loop_(loop)
    , fd_(fd__)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , logHup_(true)
    , tied_(false)
    , eventHandling_(false)
    , addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::update()
{
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_) {
        /**
         * @comment lock
         * @brief
         * - purpose: Creates a new std::shared_ptr that shares ownership of the managed object. If there is no managed object, i.e. *this is empty, 
         *            then the returned shared_ptr also is empty.
         */
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    /**
     * @question when does event handling happen
     */
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (logHup_) {
            LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_)
            closeCallback_();
    }

    /**
     * @comment poll macros 
     * @macros
     * - POLLNVAL: Invalid request: fd not open (only returned in revents; ignored in events).
     * - POLLERR: Error condition (only returned in revents; ignored in events).  This bit is also set for a file descriptor referring to the write end of 
     *            a pipe when the read end has been closed.
     * - POLLHUP: Hang up (only returned in revents; ignored in events). Note that when reading from a channel such as a pipe or a stream socket, 
     *            this event merely indicates that the peer closed its end of the channel.  Subsequent reads from the channel will return 0 (end of file) only 
     *            after all outstanding data in the channel has been consumed.
     */
    if (revents_ & POLLNVAL) {
        LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_)
            errorCallback_();
    }
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_)
            readCallback_(receiveTime);
    }
    if (revents_ & POLLOUT) {
        if (writeCallback_)
            writeCallback_();
    }
    eventHandling_ = false;
}

string Channel::reventsToString() const
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}
