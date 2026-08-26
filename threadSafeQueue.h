//
// Created by arm64 on 21.08.2026.
//

#ifndef DRONE_DAEMON_THREADSAFEQUEUE_H
#define DRONE_DAEMON_THREADSAFEQUEUE_H

#include <condition_variable>
#include <queue>
#include <mutex>
#include <thread>
#include <optional> // Обязательно для std::optional
#include <chrono>   // Обязательно для std::chrono::milliseconds
#include <iostream>


template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t maxSize) : maxSize_(maxSize) {}

    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= maxSize_) {
            queue_.pop(); // Дропаем самый старый кадр
            std::cerr << "Внимание: Очередь переполнена, кадр отброшен.\n";
        }
        queue_.push(std::move(item));
        cond_.notify_one();
    }

    // Возвращает std::nullopt, если истек таймаут (чтобы поток мог проверить флаг выхода)
    std::optional<T> pop_with_timeout(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cond_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            T val = std::move(queue_.front());
            queue_.pop();
            return val;
        }
        return std::nullopt;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    size_t maxSize_;
};

#endif //DRONE_DAEMON_THREADSAFEQUEUE_H