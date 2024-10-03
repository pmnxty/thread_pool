#include "threadpool.h"

#include <unistd.h>

#include <iostream>

void* worker_(void* arg);
void* manager_(void* arg);

ThreadPool::ThreadPool(int min, int max, int task_q_size) {
    // �̳߳ػ���������ʼ��
    max_thread_ = max;
    min_thread_ = min;
    live_thread_ = min;
    busy_thread_ = 0;
    exit_thread_ = 0;

    // ������л���������ʼ��
    task_capacity_ = task_q_size;
    task_num_ = 0;
    front_ = 0;
    tear_ = 0;

    // �����̳߳�
    shutdown_ = 0;

    // �����������
    task_q_ = new Task[task_capacity_];

    // ����ͬ������
    pthread_mutex_init(&pool_mutex_, NULL);
    pthread_mutex_init(&busy_mutex_, NULL);
    pthread_cond_init(&cant_empty_, NULL);
    pthread_cond_init(&cant_full_, NULL);

    // ���������߳�
    pthread_create(&manager_id_, NULL, manager_, this);

    work_id_ = new pthread_t[max_thread_]{0};
    // ���������߳�
    for (int i = 0; i < min_thread_; i++) {
        pthread_create(&work_id_[i], NULL, worker_, this);
    }

    std::printf("thread pool create successful...\n");
}

ThreadPool::~ThreadPool() {
    // �ر��̳߳�
    shutdown_ = 1;

    // �رչ����߳�
    pthread_join(manager_id_, NULL);

    // �رչ����߳�
    for (int i = 0; i < live_thread_; i++) {
        pthread_cond_signal(&cant_empty_);
    }

    // ������������ڴ�
    if (task_q_) delete task_q_;

    // ����ͬ������
    pthread_mutex_destroy(&pool_mutex_);
    pthread_mutex_destroy(&busy_mutex_);
    pthread_cond_destroy(&cant_empty_);
    pthread_cond_destroy(&cant_full_);

    // ���չ����߳��ڴ�
    if (work_id_) delete work_id_;

    std::printf("thread pool destroy successful...\n");
}

void ThreadPool::pool_add_task(void (*function)(void*), void* arg) {
    // �������
    pthread_mutex_lock(&pool_mutex_);

    // �������Ƿ�����
    // �����̳��Ƿ�ر�
    while (task_capacity_ <= task_num_ && !shutdown_) {
        pthread_cond_wait(&cant_full_, &pool_mutex_);
    }

    // �ж��̳߳��Ƿ�ر�
    if (shutdown_) {
        pthread_mutex_unlock(&pool_mutex_);
        return;
    }

    task_q_[tear_].function = function;
    task_q_[tear_].arg = arg;
    // ѭ������
    tear_ = (tear_ + 1) % task_capacity_;
    task_num_++;

    pthread_cond_signal(&cant_empty_);
    pthread_mutex_unlock(&pool_mutex_);
}

int ThreadPool::get_busy_num() {
    // ��ȡæµ������
    pthread_mutex_lock(&busy_mutex_);
    int val = busy_thread_;
    pthread_mutex_unlock(&busy_mutex_);
    return val;
}

int ThreadPool::get_task_num() {
    // ��ȡ��ǰ������
    pthread_mutex_lock(&pool_mutex_);
    int val = task_num_;
    pthread_mutex_unlock(&pool_mutex_);
    return val;
}

void* worker_(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    // ��ʼ����
    while (1) {
        pthread_mutex_lock(&pool->pool_mutex_);

        // �ж϶����Ƿ�Ϊ��
        // �жϽ��̳��Ƿ�ر�
        while (pool->task_num_ == 0 && !pool->shutdown_) {
            // ���������ȴ�����
            pthread_cond_wait(&pool->cant_empty_, &pool->pool_mutex_);

            // �ж��Ƿ���ɱ
            if (pool->exit_thread_ > 0) {
                pool->exit_thread_--;
                if (pool->live_thread_ > pool->min_thread_) {
                    std::printf("manager start destroy worker %lld...\n",
                                pthread_self());
                    pool->live_thread_--;
                    pthread_mutex_unlock(&pool->pool_mutex_);
                    pool->exit_thread_func_();
                }
            }
        }
        // ����̳߳��Ƿ�ر�
        if (pool->shutdown_) {
            pool->live_thread_--;
            pthread_mutex_unlock(&pool->pool_mutex_);
            pool->exit_thread_func_();
            break;
        }

        // ��ȡ����
        Task task = pool->task_q_[pool->front_];
        pool->front_ = (pool->front_ + 1) % pool->task_capacity_;
        pool->task_num_--;

        // �����������ж���������������߳�
        pthread_cond_signal(&pool->cant_full_);
        pthread_mutex_unlock(&pool->pool_mutex_);

        // ����æµ������
        pthread_mutex_lock(&pool->busy_mutex_);
        pool->busy_thread_++;
        pthread_mutex_unlock(&pool->busy_mutex_);

        // ִ������
        task.function(task.arg);

        // ����æµ������
        pthread_mutex_lock(&pool->busy_mutex_);
        pool->busy_thread_--;
        pthread_mutex_unlock(&pool->busy_mutex_);
    }

    return nullptr;
}

void* manager_(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    // ��ʼ���
    while (!pool->shutdown_) {
        // �ж��̳߳��Ƿ���Ҫ�����߳�
        pthread_mutex_lock(&pool->pool_mutex_);

        // ����������Ƿ���ڴ���߳���
        if (pool->task_num_ > pool->live_thread_ &&
            pool->live_thread_ < pool->max_thread_) {
            for (int i = 0, count = 0;
                 i < pool->max_thread_ && count < pool->per_change_num_ &&
                 pool->live_thread_ < pool->max_thread_;
                 i++) {
                if (pool->work_id_[i] == 0) {
                    pthread_create(&pool->work_id_[i], NULL, worker_, arg);
                    pool->live_thread_++;
                    count++;
                    std::printf("manager thread create new work thread...\n");
                }
            }
        }
        pthread_mutex_unlock(&pool->pool_mutex_);
        // ����̳߳��Ƿ���Ҫ�����߳�
        pthread_mutex_lock(&pool->pool_mutex_);
        pthread_mutex_lock(&pool->busy_mutex_);

        // ����������Ƿ�ԶС�ڴ�������
        if ((2 * pool->task_num_) < pool->live_thread_ &&
            pool->live_thread_ > pool->min_thread_) {
            pool->exit_thread_ += pool->per_change_num_;
            for (int i = 0; i < pool->per_change_num_; i++) {
                pthread_cond_signal(&pool->cant_empty_);
            }
        }

        pthread_mutex_unlock(&pool->busy_mutex_);
        pthread_mutex_unlock(&pool->pool_mutex_);

        sleep(2);
    }
    return nullptr;
}

void ThreadPool::exit_thread_func_() {
    pthread_t tid = pthread_self();
    int isClose = 0;

    for (int i = 0; i < max_thread_; i++) {
        if (work_id_[i] == tid) {
            work_id_[i] = 0;
            isClose = 1;
            break;
        }
    }
    if (isClose) {
        std::printf("thread %lld start destroy...\n", pthread_self());
        pthread_exit(NULL);
    }
    return;
}
