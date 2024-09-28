#include "threadpool.h"

#include <pthread.h>
#include <unistd.h>

#include <string>

#define ADD_THREAD_PER 2

// 任务结构体
typedef struct Task {
    void (*function)(void* arg);
    void* arg;
} Task;

struct ThreadPool {
    // 任务队列由循环队列实现
    Task* task_q;       // 任务队列
    int task_num;       // 当前任务数量
    int task_capacity;  // 任务队列容量容量
    int task_front;     // 任务队列头指针
    int task_tear;      // 任务队列尾指针

    // 线程管理
    pthread_t manager_id;  // 管理线程
    pthread_t* work_id;    // 工作线程
    int thread_min_num;    // 最小线程数
    int thread_max_num;    // 最大线程数
    int thread_busy_num;   // 忙碌线程数
    int thread_live_num;   // 存活线程数
    int thread_exit_num;   // 退出线程数

    // 同步机制
    pthread_mutex_t mutex;       // 线程池互斥锁
    pthread_mutex_t busy_mutex;  // thread_busy_num 互斥锁
    pthread_cond_t not_full;     // 满任务队列条件变量
    pthread_cond_t not_empty;    // 空任务队列条件变量

    int shutdown;  // 是否销毁线程，销毁为1，不销毁为0
};

ThreadPool* create_thread_pool(int min, int max, int task_capacity) {
    // 记录初始化线程池变量是否成功
    int cwn = 0, cmn = -1, mf = -1, bmf = -1, nff = -1, nef = -1;
    ThreadPool* thread_pool;

    do {
        // 判断参数是否规范
        if (min < 1 || max < 1 || task_capacity < 1 || min >= max) {
            std::printf("error:[Error params in create_thread_pool]\n");
            break;
        }

        // 创建线程池
        thread_pool = new ThreadPool();
        if (thread_pool == nullptr) {
            std::printf("error:[Thread pool create fail]\n");
            break;
        }

        // 创建任务队列并初始化
        thread_pool->task_q = new Task[task_capacity];
        if (thread_pool->task_q == nullptr) {
            std::printf("error:[Task queue create fail]\n");
            break;
        }
        thread_pool->task_capacity = task_capacity;
        thread_pool->task_num = 0;
        thread_pool->task_front = 0;
        thread_pool->task_tear = 0;

        thread_pool->thread_max_num = max;
        thread_pool->thread_min_num = min;
        thread_pool->thread_busy_num = 0;
        thread_pool->thread_live_num = min;
        thread_pool->thread_exit_num = 0;

        thread_pool->shutdown = 0;

        // 同步机制初始化
        if ((mf = pthread_mutex_init(&thread_pool->mutex, NULL)) != 0 ||
            (bmf = pthread_mutex_init(&thread_pool->busy_mutex, NULL)) != 0 ||
            (nff = pthread_cond_init(&thread_pool->not_full, NULL)) != 0 ||
            (nef = pthread_cond_init(&thread_pool->not_empty, NULL)) != 0) {
            std::printf("error:[Synchronize init fail]\n");
            break;
        }

        // 创建工作线程
        thread_pool->work_id = new pthread_t[max];
        if (thread_pool->work_id == nullptr) {
            std::printf("error:[Work thread queue create fail]\n");
            break;
        }
        int wib = 0;
        for (int i = 0; i < min; i++) {
            int f = pthread_create(&thread_pool->work_id[i], NULL, worker,
                                   thread_pool);
            if (f != 0) {
                std::printf("error:[Worker thread(%d) create fail]\n", i);
                wib = 1;
                break;
            }
            std::printf("work thread %d was created...\n",
                        thread_pool->work_id[i]);
            cwn++;
        }
        if (wib) break;

        // 创建管理线程
        cmn = pthread_create(&thread_pool->manager_id, NULL, manager,
                             thread_pool);
        if (cmn != 0) {
            std::printf("error:[Manager thread create fail]\n");
            break;
        }
        std::printf("manager thread %d was created...\n", pthread_self());

        std::printf("thread pool init success...\n");
        return thread_pool;

    } while (0);

    if (thread_pool) {
        if (thread_pool->task_q) delete thread_pool->task_q;
        if (mf == 0) pthread_mutex_destroy(&thread_pool->mutex);
        if (bmf == 0) pthread_mutex_destroy(&thread_pool->busy_mutex);
        if (nef == 0) pthread_cond_destroy(&thread_pool->not_empty);
        if (nff == 0) pthread_cond_destroy(&thread_pool->not_full);
        if (cmn == 0) pthread_cancel(thread_pool->manager_id);
        if (thread_pool->work_id) {
            for (int i = 0; i < cwn; i++) {
                pthread_cancel(thread_pool->work_id[i]);
            }
            delete thread_pool->work_id;
        }
    }
    if (thread_pool) delete thread_pool;
    std::printf("thread pool fail...\n");

    return nullptr;
}

void* worker(void* arg) {
    ThreadPool* thread_pool = (ThreadPool*)arg;
    std::printf("work thread %d is in worker...\n", pthread_self());

    while (1) {
        // 加锁，访问任务队列
        pthread_mutex_lock(&thread_pool->mutex);
        // 筛选空队列或关闭线程池状况
        while (thread_pool->task_num == 0 && !thread_pool->shutdown) {
            // 线程休眠
            pthread_cond_wait(&thread_pool->not_empty, &thread_pool->mutex);
            // 有空闲线程需要销毁
            if (thread_pool->thread_exit_num > 0) {
                thread_pool->thread_exit_num--;
                // 判断存活线程数是否大于最小线程数
                if (thread_pool->thread_live_num >
                    thread_pool->thread_min_num) {
                    thread_exit(thread_pool);
                }
            }
        }
        // 如果线程池关闭，则销毁线程
        if (thread_pool->shutdown) {
            thread_exit(thread_pool);
        }

        // 取出队列中第一个任务
        Task task;
        task.function = thread_pool->task_q[thread_pool->task_front].function;
        task.arg = thread_pool->task_q[thread_pool->task_front].arg;
        // 移动指针（任务队列为环形队列）
        thread_pool->task_front =
            (thread_pool->task_front + 1) % thread_pool->task_capacity;
        // 任务队列任务数减一
        thread_pool->task_num--;

        // 解锁，退出访问任务队列
        pthread_cond_signal(&thread_pool->not_full);
        pthread_mutex_unlock(&thread_pool->mutex);

        // 忙线程加一
        pthread_mutex_lock(&thread_pool->busy_mutex);
        thread_pool->thread_busy_num++;
        pthread_mutex_unlock(&thread_pool->busy_mutex);

        // 执行
        task.function(task.arg);

        // 忙线程减一
        pthread_mutex_lock(&thread_pool->busy_mutex);
        thread_pool->thread_busy_num--;
        pthread_mutex_unlock(&thread_pool->busy_mutex);
    }

    return NULL;
}

void* manager(void* arg) {
    ThreadPool* thread_pool = (ThreadPool*)arg;

    while (!thread_pool->shutdown) {
        sleep(1);
        // 获取线程池属性
        pthread_mutex_lock(&thread_pool->mutex);
        int task_num = thread_pool->task_num;
        int thread_live_num = thread_pool->thread_live_num;
        int thread_max_num = thread_pool->thread_max_num;
        int thread_min_num = thread_pool->thread_min_num;

        // 如果任务书大于存活进程数，则创建线程
        if (task_num > thread_live_num && thread_live_num < thread_max_num) {
            for (int i = 0, count = 0;
                 i < thread_max_num && count < ADD_THREAD_PER &&
                 thread_live_num < thread_max_num;
                 i++) {
                // pthread_t 为0，说明此处有空位
                if (thread_pool->work_id[i] == 0) {
                    pthread_create(&thread_pool->work_id[i], NULL, worker,
                                   thread_pool);
                    thread_live_num++;
                    thread_pool->thread_live_num++;
                    count++;
                    std::printf(
                        "manager thread %d add new work thread %d ...\n",
                        pthread_self(), thread_pool->work_id[i]);
                }
            }
        }
        pthread_mutex_unlock(&thread_pool->mutex);

        // 如果任务数远小于存活进程数，则销毁一些进程
        if ((task_num * 2) < thread_live_num &&
            thread_live_num > thread_min_num) {
            pthread_mutex_lock(&thread_pool->mutex);
            thread_pool->thread_exit_num += ADD_THREAD_PER;
            pthread_mutex_unlock(&thread_pool->mutex);

            // 唤醒工作线程，其根据线程池 thread_exit_num 自杀
            for (int i = 0; i < ADD_THREAD_PER; i++) {
                pthread_cond_signal(&thread_pool->not_empty);
                std::printf("too many work thread live\n");
            }
        }
    }

    return NULL;
}

void thread_exit(ThreadPool* thread_pool) {
    pthread_t tid = pthread_self();

    for (int i = 0; i < thread_pool->thread_max_num; i++) {
        // 查找线程本身所在位置并清空
        if (thread_pool->work_id[i] == tid) {
            thread_pool->work_id[i] = 0;
            thread_pool->thread_live_num--;
            break;
        }
    }
    std::printf("work thread %d exit...\n", pthread_self());
    pthread_mutex_unlock(&thread_pool->mutex);
    pthread_exit(NULL);
    return;
}

void thread_pool_add_task(ThreadPool* thread_pool, void (*func)(void*),
                          void* arg) {
    pthread_mutex_lock(&thread_pool->mutex);
    // 线程池任务队列已满
    while (thread_pool->task_num >= thread_pool->task_capacity &&
           !thread_pool->shutdown) {
        std::printf("too many tasks, watting...\n");
        pthread_cond_wait(&thread_pool->not_full, &thread_pool->mutex);
    }

    // 如果线程池关闭，则直接返回
    if (thread_pool->shutdown) {
        pthread_mutex_unlock(&thread_pool->mutex);
        return;
    }

    // 添加任务
    thread_pool->task_q[thread_pool->task_tear].function = func;
    thread_pool->task_q[thread_pool->task_tear].arg = arg;
    thread_pool->task_tear =
        (thread_pool->task_tear + 1) % thread_pool->task_capacity;
    thread_pool->task_num++;
    std::printf("add task in thread pool success...\n");
    // 唤醒因任务队列为空而阻塞的线程
    pthread_cond_signal(&thread_pool->not_empty);
    pthread_mutex_unlock(&thread_pool->mutex);
    return;
}

// 获取 thread_busy_num
int get_thread_busy_num(ThreadPool* thread_pool) {
    int val = 0;
    pthread_mutex_lock(&thread_pool->busy_mutex);
    val = thread_pool->thread_busy_num;
    pthread_mutex_unlock(&thread_pool->busy_mutex);
    return val;
}

// 获取 thread_live_num
int get_thread_live_num(ThreadPool* thread_pool) {
    int val = 0;
    pthread_mutex_lock(&thread_pool->mutex);
    val = thread_pool->thread_live_num;
    pthread_mutex_unlock(&thread_pool->mutex);
    return val;
}

// 销毁线程池
int destroy_thread_pool(ThreadPool* thread_pool) {
        // 判断线程池是否为空
    if (thread_pool == nullptr) {
        std::printf("error:[Destroy thread pool fail. The param is nullptr]\n");
        return -1;
    }
    thread_pool->shutdown = 1;
    // 销毁管理线程
    pthread_join(thread_pool->manager_id, NULL);
    // 销毁工作线程，其被唤醒并自杀
    for (int i = 0; i < thread_pool->thread_live_num; i++) {
        pthread_cond_signal(&thread_pool->not_empty);
    }
    // 由于未知的原因，在这里不睡2秒，就有 thread_min_num 条线程无法回收
    sleep(2);
    
    // 回收任务队列内存
    if (thread_pool->task_q) {
        delete thread_pool->task_q;
        thread_pool->task_q = nullptr;
    }
    // 回收工作线程内存
    if (thread_pool->work_id) {
        delete thread_pool->work_id;
        thread_pool->work_id = nullptr;
    }
    // 销毁互斥锁和条件变量
    pthread_mutex_destroy(&thread_pool->mutex);
    pthread_mutex_destroy(&thread_pool->busy_mutex);
    pthread_cond_destroy(&thread_pool->not_empty);
    pthread_cond_destroy(&thread_pool->not_full);
    // 回收线程池内存
    delete thread_pool;
    thread_pool = nullptr;
    std::printf("thread pool free success...\n");

    return 0;
}
