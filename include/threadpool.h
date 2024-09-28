#pragma once
#include <iostream>

typedef struct ThreadPool ThreadPool;

// �����̳߳ز���ʼ��
ThreadPool* create_thread_pool(int min, int max, int task_capacity);

// �����̳߳�
int destroy_thread_pool(ThreadPool* thread_pool);

// ���̳߳��������
void thread_pool_add_task(ThreadPool* thread_pool, void (*func)(void*),
                          void* arg);

// ��ȡ�̳߳��й������߳���
int get_thread_busy_num(ThreadPool* thread_pool);

// ��ȡ�̳߳��д����߳���
int get_thread_live_num(ThreadPool* thread_pool);

// �ڲ�����
// �����̺߳���
void* worker(void* arg);

// �����̺߳���
void* manager(void* arg);

// �߳���ɱ
void thread_exit(ThreadPool* thread_pool);