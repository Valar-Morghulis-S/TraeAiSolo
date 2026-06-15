#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    template<typename F>
    void enqueue(F&& task);

    size_t getThreadCount() const { return workers_.size(); }
    void waitAll();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    std::atomic<int> active_tasks_;
    std::mutex wait_mutex_;
    std::condition_variable wait_condition_;
};

template<typename F>
void ThreadPool::enqueue(F&& task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace(std::forward<F>(task));
    }
    condition_.notify_one();
}

#endif // THREAD_POOL_H