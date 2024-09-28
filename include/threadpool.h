#pragma once
#include <iostream>

typedef struct ThreadPool ThreadPool;

// 创建线程池并初始化
ThreadPool* create_thread_pool(int min, int max, int task_capacity);

// 销毁线程池
int destroy_thread_pool(ThreadPool* thread_pool);

// 给线程池添加任务
void thread_pool_add_task(ThreadPool* thread_pool, void (*func)(void*),
                          void* arg);

// 获取线程池中工作的线程数
int get_thread_busy_num(ThreadPool* thread_pool);

// 获取线程池中存活的线程数
int get_thread_live_num(ThreadPool* thread_pool);

// 内部函数
// 工作线程函数
void* worker(void* arg);

// 管理线程函数
void* manager(void* arg);

// 线程自杀
void thread_exit(ThreadPool* thread_pool);