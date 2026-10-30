#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

// thread-safe FIFO queue used to hand work between pipeline threads
// pop blocks until an item is available, and returns nullopt once the queue is closed and drained
template <typename T>
class BlockingQueue final {
    private:
        std::mutex mutex_;
        std::condition_variable not_empty_;
        std::deque<T> items_;
        bool closed_{false};

    public:
        void push(T item) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                items_.push_back(std::move(item));
            }
            not_empty_.notify_one();
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(mutex_);
            not_empty_.wait(lock, [this] { return !items_.empty() || closed_; });
            if (items_.empty()) {
                return std::nullopt;
            }
            T item = std::move(items_.front());
            items_.pop_front();
            return item;
        }

        void close() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                closed_ = true;
            }
            not_empty_.notify_all();
        }
};
