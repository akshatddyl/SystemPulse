#ifndef CONCURRENT_QUEUE_HPP
#define CONCURRENT_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <atomic>

template <typename T>
class ConcurrentQueue {
public:
    explicit ConcurrentQueue(size_t max_capacity) : max_capacity_(max_capacity) {}

    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_capacity_) {
            // Drop oldest data to prevent OOM
            queue_.pop();
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
        }
        queue_.push(item);
        cond_var_.notify_one();
    }

    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_capacity_) {
            // Drop oldest data to prevent OOM
            queue_.pop();
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
        }
        queue_.push(std::move(item));
        cond_var_.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this]() { return !queue_.empty(); });
        
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    uint64_t dropped_count() const {
        return dropped_count_.load(std::memory_order_relaxed);
    }

private:
    std::queue<T> queue_;
    size_t max_capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::atomic<uint64_t> dropped_count_{0};
};

#endif // CONCURRENT_QUEUE_HPP
