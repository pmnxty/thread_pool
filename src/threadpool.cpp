#include "threadpool.h"

#include <chrono>
#include <iostream>

ThreadPool::ThreadPool(int min, int max)
    : max_thread(max),
      min_thread(min),
      cur_thread(min),
      free_thread(min),
      exit_thread(0),
      shutdown(false) {
    // 创建管理线程
    manager_id = std::thread(&ThreadPool::manager, this);
    // 创建工作线程
    for (int i = 0; i < cur_thread; i++) {
        std::thread tmp_t(&ThreadPool::worker, this);
        work_ids.insert(make_pair(tmp_t.get_id(), std::move(tmp_t)));
    }
}

ThreadPool::~ThreadPool() {
    shutdown.store(true);
    std::cout << "thread pool is shutdown" << std::endl;
    not_empty.notify_all();
    exit_thread += cur_thread.load();
    for (auto it = work_ids.begin(); it != work_ids.end(); it++) {
        std::cout << "work thread " << it->first << " is destroy" << std::endl;
        if (it->second.joinable()) it->second.join();
    }
    if (manager_id.joinable()) manager_id.join();
    std::cout << "manager thread is destroy" << std::endl;
}

void ThreadPool::worker() {
    while (!shutdown.load()) {
        std::function<void()> task = nullptr;
        {
            std::unique_lock<std::mutex> lck(mutex_t);
            while(!shutdown.load() && task_q.size() <= 0){
                std::cout << "task is empty, work thread need to wait..." << std::endl;
                not_empty.wait(lck);

                // 如果需要销毁线程
                if (exit_thread > 0) {
                    exit_thread--;
                    if (cur_thread > min_thread) {
                        cur_thread--;
                        free_thread--;
                        std::cout << "work thread " << std::this_thread::get_id()
                                << " register destroy queue" << std::endl;
                        lck.unlock();
                        std::lock_guard<std::mutex> elck(mutex_exit);
                        // 登记销毁队列
                        exit_works.emplace_back(std::this_thread::get_id());
                        return;
                    }
                }
            }
            // 如果线程池关闭
            if (shutdown.load()) return;

            if(task_q.size() > 0){
                task = std::move(task_q.front());
                task_q.pop();
            }
        }

        if (task) {
            free_thread--;
            task();
            free_thread++;
        }
    }
}

void ThreadPool::manager() {
    while (!shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::unique_lock<std::mutex> lck(mutex_t);
        auto task_size = task_q.size();
        std::cout << "manager start working, task num is " << task_size
                  << ", current thread num is " << cur_thread
                  << ", free thread num is " << free_thread
                  << ", exit thread num is " << exit_thread << std::endl;
        if (free_thread < task_size && cur_thread < max_thread) {
            auto tmp_t = std::thread(&ThreadPool::worker, this);
            std::cout << "work thread is shortage, thread " << tmp_t.get_id()
                      << " is created..." << std::endl;
            work_ids.insert(make_pair(tmp_t.get_id(), std::move(tmp_t)));
            cur_thread++;
            free_thread++;
        } else if ((free_thread / 2) > task_size && cur_thread > min_thread) {
            exit_thread += PER_CHANGE_NUM;
            for (int i = 0; i < PER_CHANGE_NUM; i++) {
                std::cout << "manager thread notify one work thread..." << std::endl;
                not_empty.notify_one();
            }
        }
        std::lock_guard<std::mutex> elck(mutex_exit);
        while (exit_works.size() > 0) {
            std::cout << "destroy queue num is " << exit_works.size() << std::endl;
            for (auto it = exit_works.begin(); it != exit_works.end();) {
                auto tmp_t = work_ids.find(*it);
                if (tmp_t->second.joinable()) tmp_t->second.join();
                work_ids.erase(tmp_t);
                std::cout << "manager thread destroy work thread " << *it
                          << std::endl;
                exit_works.erase(it);
                it = exit_works.begin();
            }
        }
    }
}