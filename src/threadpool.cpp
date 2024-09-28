#include "threadpool.h"

#include <pthread.h>
#include <unistd.h>

#include <string>

#define ADD_THREAD_PER 2

// ����ṹ��
typedef struct Task {
    void (*function)(void* arg);
    void* arg;
} Task;

struct ThreadPool {
    // ���������ѭ������ʵ��
    Task* task_q;       // �������
    int task_num;       // ��ǰ��������
    int task_capacity;  // ���������������
    int task_front;     // �������ͷָ��
    int task_tear;      // �������βָ��

    // �̹߳���
    pthread_t manager_id;  // �����߳�
    pthread_t* work_id;    // �����߳�
    int thread_min_num;    // ��С�߳���
    int thread_max_num;    // ����߳���
    int thread_busy_num;   // æµ�߳���
    int thread_live_num;   // ����߳���
    int thread_exit_num;   // �˳��߳���

    // ͬ������
    pthread_mutex_t mutex;       // �̳߳ػ�����
    pthread_mutex_t busy_mutex;  // thread_busy_num ������
    pthread_cond_t not_full;     // �����������������
    pthread_cond_t not_empty;    // �����������������

    int shutdown;  // �Ƿ������̣߳�����Ϊ1��������Ϊ0
};

ThreadPool* create_thread_pool(int min, int max, int task_capacity) {
    // ��¼��ʼ���̳߳ر����Ƿ�ɹ�
    int cwn = 0, cmn = -1, mf = -1, bmf = -1, nff = -1, nef = -1;
    ThreadPool* thread_pool;

    do {
        // �жϲ����Ƿ�淶
        if (min < 1 || max < 1 || task_capacity < 1 || min >= max) {
            std::printf("error:[Error params in create_thread_pool]\n");
            break;
        }

        // �����̳߳�
        thread_pool = new ThreadPool();
        if (thread_pool == nullptr) {
            std::printf("error:[Thread pool create fail]\n");
            break;
        }

        // ����������в���ʼ��
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

        // ͬ�����Ƴ�ʼ��
        if ((mf = pthread_mutex_init(&thread_pool->mutex, NULL)) != 0 ||
            (bmf = pthread_mutex_init(&thread_pool->busy_mutex, NULL)) != 0 ||
            (nff = pthread_cond_init(&thread_pool->not_full, NULL)) != 0 ||
            (nef = pthread_cond_init(&thread_pool->not_empty, NULL)) != 0) {
            std::printf("error:[Synchronize init fail]\n");
            break;
        }

        // ���������߳�
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

        // ���������߳�
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
        // �����������������
        pthread_mutex_lock(&thread_pool->mutex);
        // ɸѡ�ն��л�ر��̳߳�״��
        while (thread_pool->task_num == 0 && !thread_pool->shutdown) {
            // �߳�����
            pthread_cond_wait(&thread_pool->not_empty, &thread_pool->mutex);
            // �п����߳���Ҫ����
            if (thread_pool->thread_exit_num > 0) {
                thread_pool->thread_exit_num--;
                // �жϴ���߳����Ƿ������С�߳���
                if (thread_pool->thread_live_num >
                    thread_pool->thread_min_num) {
                    thread_exit(thread_pool);
                }
            }
        }
        // ����̳߳عرգ��������߳�
        if (thread_pool->shutdown) {
            thread_exit(thread_pool);
        }

        // ȡ�������е�һ������
        Task task;
        task.function = thread_pool->task_q[thread_pool->task_front].function;
        task.arg = thread_pool->task_q[thread_pool->task_front].arg;
        // �ƶ�ָ�루�������Ϊ���ζ��У�
        thread_pool->task_front =
            (thread_pool->task_front + 1) % thread_pool->task_capacity;
        // ���������������һ
        thread_pool->task_num--;

        // �������˳������������
        pthread_cond_signal(&thread_pool->not_full);
        pthread_mutex_unlock(&thread_pool->mutex);

        // æ�̼߳�һ
        pthread_mutex_lock(&thread_pool->busy_mutex);
        thread_pool->thread_busy_num++;
        pthread_mutex_unlock(&thread_pool->busy_mutex);

        // ִ��
        task.function(task.arg);

        // æ�̼߳�һ
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
        // ��ȡ�̳߳�����
        pthread_mutex_lock(&thread_pool->mutex);
        int task_num = thread_pool->task_num;
        int thread_live_num = thread_pool->thread_live_num;
        int thread_max_num = thread_pool->thread_max_num;
        int thread_min_num = thread_pool->thread_min_num;

        // �����������ڴ����������򴴽��߳�
        if (task_num > thread_live_num && thread_live_num < thread_max_num) {
            for (int i = 0, count = 0;
                 i < thread_max_num && count < ADD_THREAD_PER &&
                 thread_live_num < thread_max_num;
                 i++) {
                // pthread_t Ϊ0��˵���˴��п�λ
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

        // ���������ԶС�ڴ���������������һЩ����
        if ((task_num * 2) < thread_live_num &&
            thread_live_num > thread_min_num) {
            pthread_mutex_lock(&thread_pool->mutex);
            thread_pool->thread_exit_num += ADD_THREAD_PER;
            pthread_mutex_unlock(&thread_pool->mutex);

            // ���ѹ����̣߳�������̳߳� thread_exit_num ��ɱ
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
        // �����̱߳�������λ�ò����
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
    // �̳߳������������
    while (thread_pool->task_num >= thread_pool->task_capacity &&
           !thread_pool->shutdown) {
        std::printf("too many tasks, watting...\n");
        pthread_cond_wait(&thread_pool->not_full, &thread_pool->mutex);
    }

    // ����̳߳عرգ���ֱ�ӷ���
    if (thread_pool->shutdown) {
        pthread_mutex_unlock(&thread_pool->mutex);
        return;
    }

    // �������
    thread_pool->task_q[thread_pool->task_tear].function = func;
    thread_pool->task_q[thread_pool->task_tear].arg = arg;
    thread_pool->task_tear =
        (thread_pool->task_tear + 1) % thread_pool->task_capacity;
    thread_pool->task_num++;
    std::printf("add task in thread pool success...\n");
    // �������������Ϊ�ն��������߳�
    pthread_cond_signal(&thread_pool->not_empty);
    pthread_mutex_unlock(&thread_pool->mutex);
    return;
}

// ��ȡ thread_busy_num
int get_thread_busy_num(ThreadPool* thread_pool) {
    int val = 0;
    pthread_mutex_lock(&thread_pool->busy_mutex);
    val = thread_pool->thread_busy_num;
    pthread_mutex_unlock(&thread_pool->busy_mutex);
    return val;
}

// ��ȡ thread_live_num
int get_thread_live_num(ThreadPool* thread_pool) {
    int val = 0;
    pthread_mutex_lock(&thread_pool->mutex);
    val = thread_pool->thread_live_num;
    pthread_mutex_unlock(&thread_pool->mutex);
    return val;
}

// �����̳߳�
int destroy_thread_pool(ThreadPool* thread_pool) {
        // �ж��̳߳��Ƿ�Ϊ��
    if (thread_pool == nullptr) {
        std::printf("error:[Destroy thread pool fail. The param is nullptr]\n");
        return -1;
    }
    thread_pool->shutdown = 1;
    // ���ٹ����߳�
    pthread_join(thread_pool->manager_id, NULL);
    // ���ٹ����̣߳��䱻���Ѳ���ɱ
    for (int i = 0; i < thread_pool->thread_live_num; i++) {
        pthread_cond_signal(&thread_pool->not_empty);
    }
    // ����δ֪��ԭ�������ﲻ˯2�룬���� thread_min_num ���߳��޷�����
    sleep(2);
    
    // ������������ڴ�
    if (thread_pool->task_q) {
        delete thread_pool->task_q;
        thread_pool->task_q = nullptr;
    }
    // ���չ����߳��ڴ�
    if (thread_pool->work_id) {
        delete thread_pool->work_id;
        thread_pool->work_id = nullptr;
    }
    // ���ٻ���������������
    pthread_mutex_destroy(&thread_pool->mutex);
    pthread_mutex_destroy(&thread_pool->busy_mutex);
    pthread_cond_destroy(&thread_pool->not_empty);
    pthread_cond_destroy(&thread_pool->not_full);
    // �����̳߳��ڴ�
    delete thread_pool;
    thread_pool = nullptr;
    std::printf("thread pool free success...\n");

    return 0;
}
