#include "thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t numThreads)
    : stop_(false), active_tasks_(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_.load() || !tasks_.empty();
                    });
                    if (stop_.load() && tasks_.empty()) {
                        return;
                    }
                    if (!tasks_.empty()) {
                        task = std::move(tasks_.front());
                        tasks_.pop();
                        ++active_tasks_;
                    }
                }
                if (task) {
                    task();
                    {
                        std::lock_guard<std::mutex> lock(wait_mutex_);
                        --active_tasks_;
                        wait_condition_.notify_all();
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    stop_.store(true);
    condition_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::waitAll() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_condition_.wait(lock, [this] {
        std::lock_guard<std::mutex> q_lock(queue_mutex_);
        return tasks_.empty() && active_tasks_.load() == 0;
    });
}