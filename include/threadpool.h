#pragma once
#include <pthread.h>

typedef struct Task {
    void (*function)(void* arg);
    void* arg;
} Task;

class ThreadPool {
   public:
    // 初始化线程池
    ThreadPool(int min, int max, int task_q_size);

    // 销毁线程池
    ~ThreadPool();

    // 添加任务
    void pool_add_task(void (*func)(void*), void* arg);

    int get_busy_num();

    int get_task_num();

    private:

    friend void* worker_(void* arg);
    friend void* manager_(void* arg);
    
    void exit_thread_func_();

   private:
    Task* task_q_;       // 任务队列
    int task_capacity_;  // 队列容量
    int task_num_;       // 当前任务数
    int front_;          // 队列头指针
    int tear_;           // 队列尾指针

    pthread_t manager_id_;  // 管理线程
    pthread_t* work_id_;    // 工作线程
    int max_thread_;        // 最大线程数
    int min_thread_;        // 最小线程数
    int busy_thread_;       // 忙碌线程数
    int live_thread_;       // 存活线程数
    int exit_thread_;       // 退出线程数

    pthread_mutex_t pool_mutex_;  // 线程池互斥锁
    pthread_mutex_t busy_mutex_;  // busy_thread 互斥锁
    pthread_cond_t cant_full_;    // 任务队列不能满 条件变量
    pthread_cond_t cant_empty_;   // 任务队列不能空 条件变量

    int shutdown_;  // 是否关闭，1关闭，0不关闭

    const int per_change_num_ = 2;
};
