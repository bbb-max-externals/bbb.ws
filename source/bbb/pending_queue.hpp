#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <utility>

namespace bbb {
template<class Message>
class pending_queue {
public:
    using clock = std::chrono::steady_clock;
    static constexpr std::size_t capacity = 64;
    struct batch {
        std::deque<Message> messages;
        bool report_drops{};
    };

    void push(Message message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(messages_.size() == capacity) {
            messages_.pop_front();
            dropped_ = true;
        }
        messages_.push_back(std::move(message));
    }

    batch drain(clock::time_point now = clock::now()) {
        batch result;
        std::lock_guard<std::mutex> lock(mutex_);
        result.messages.swap(messages_);
        if(dropped_ && next_report_ <= now) {
            result.report_drops = true;
            dropped_ = false;
            next_report_ = now + std::chrono::seconds(1);
        }
        return result;
    }
private:
    std::mutex mutex_;
    std::deque<Message> messages_;
    bool dropped_{};
    clock::time_point next_report_{clock::time_point::min()};
};
}
