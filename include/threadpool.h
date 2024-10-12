#pragma once
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
#define PER_CHANGE_NUM 2
    using thread_id = decltype(std::this_thread::get_id());

   public:
    ThreadPool(int min, int max = std::thread::hardware_concurrency());
    ~ThreadPool();
    template <typename Func, typename... Args>
    auto commit(Func&& func,
                Args&&... args) -> std::future<decltype(func(args...))> {
        using res_type = decltype(func(args...));

        auto task = std::make_shared<std::packaged_task<res_type()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        {
            std::lock_guard<std::mutex> lck(mutex_t);
            task_q.emplace([task] { (*task)(); });
        }
        not_empty.notify_one();
        return task->get_future();
    }

   private:
    void worker();
    void manager();

   private:
    std::queue<std::function<void()>> task_q;

    std::thread manager_id;
    std::map<thread_id, std::thread> work_ids;
    std::vector<thread_id> exit_works;
    std::mutex mutex_exit;
    std::mutex mutex_t;
    std::condition_variable not_empty;

    size_t max_thread;
    size_t min_thread;
    std::atomic<size_t> cur_thread;
    std::atomic<size_t> free_thread;
    std::atomic<size_t> exit_thread;
    std::atomic<bool> shutdown;
};

#endif  // THREAD_POOL_H