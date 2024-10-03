#pragma once
#include <pthread.h>

typedef struct Task {
    void (*function)(void* arg);
    void* arg;
} Task;

class ThreadPool {
   public:
    // ��ʼ���̳߳�
    ThreadPool(int min, int max, int task_q_size);

    // �����̳߳�
    ~ThreadPool();

    // �������
    void pool_add_task(void (*func)(void*), void* arg);

    int get_busy_num();

    int get_task_num();

    private:

    friend void* worker_(void* arg);
    friend void* manager_(void* arg);
    
    void exit_thread_func_();

   private:
    Task* task_q_;       // �������
    int task_capacity_;  // ��������
    int task_num_;       // ��ǰ������
    int front_;          // ����ͷָ��
    int tear_;           // ����βָ��

    pthread_t manager_id_;  // �����߳�
    pthread_t* work_id_;    // �����߳�
    int max_thread_;        // ����߳���
    int min_thread_;        // ��С�߳���
    int busy_thread_;       // æµ�߳���
    int live_thread_;       // ����߳���
    int exit_thread_;       // �˳��߳���

    pthread_mutex_t pool_mutex_;  // �̳߳ػ�����
    pthread_mutex_t busy_mutex_;  // busy_thread ������
    pthread_cond_t cant_full_;    // ������в����� ��������
    pthread_cond_t cant_empty_;   // ������в��ܿ� ��������

    int shutdown_;  // �Ƿ�رգ�1�رգ�0���ر�

    const int per_change_num_ = 2;
};
