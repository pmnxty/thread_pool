#include "threadpool.h"
#include <iostream>
#include <unistd.h>
Task::Task() {
    function = nullptr;
    arg = nullptr;
}

Task::Task(callback func, void* a) {
    function = func;
    arg = a;
}

ThreadPool::ThreadPool(int min, int max) {
    do {
        shutdown = false;
        min_thread = min;
        live_thread = min;
        max_thread = max;
        busy_thread = 0;
        exit_thread = 0;

        if (pthread_mutex_init(&busy_mutex, NULL) != 0 ||
            pthread_mutex_init(&mutex, NULL) != 0 ||
            pthread_cond_init(&cant_empty, NULL) != 0 ) {
            break;
        }

        worker_ids = new pthread_t[max_thread];
        int bf = 0;
        for(long_int i = 0; i < min_thread; i++){
            if(pthread_create(&worker_ids[i], NULL, worker_, this) != 0){
                bf = 1;
                break;
            }
        }
        if(bf) break;
        if(pthread_create(&manager_id, NULL, manager_, this) != 0) break;
        
        printf("thread pool init successful\n");
        return;
    } while(0);
    printf("thread pool init fail\n");
}

ThreadPool::~ThreadPool(){
    shutdown = true;
    for(long_int i = 0; i < live_thread; i++){
        pthread_cond_signal(&cant_empty);
    }
    pthread_join(manager_id, NULL);
    if(worker_ids) delete worker_ids;
    pthread_cond_destroy(&cant_empty);
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&busy_mutex);

    printf("thread pool destroy\n");
}

void ThreadPool::add_task(Task task) {
    pthread_mutex_lock(&mutex);
    task_q.push(task);
    pthread_cond_signal(&cant_empty);
    pthread_mutex_unlock(&mutex);
}

long_int ThreadPool::get_busy(){
    long_int num;
    pthread_mutex_lock(&mutex);
    num = busy_thread;
    pthread_mutex_unlock(&mutex);
    return num;
}

long_int ThreadPool::get_live(){
    long_int num;
    pthread_mutex_lock(&mutex);
    num = live_thread;
    pthread_mutex_unlock(&mutex);
    return num;
}

void ThreadPool::thread_exit_(){
    pthread_t tid = pthread_self();
    for(long_int i = 0; i < max_thread; i++){
        if(worker_ids[i] == tid){
            worker_ids[i] = 0;
            pthread_exit(NULL);
        }
    }
}

void* ThreadPool::worker_(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;

    while(1){
        pthread_mutex_lock(&pool->mutex);

        while(pool->task_q.size() <= 0 && !pool->shutdown){
            pthread_cond_wait(&pool->cant_empty, &pool->mutex);
            if(pool->exit_thread > 0){
                pool->exit_thread--;
                if(pool->live_thread > pool->min_thread){
                    pool->live_thread--;
                    pthread_mutex_unlock(&pool->mutex);
                    printf("manager destroy worker %ld\n", pthread_self());
                    pool->thread_exit_();        
                }
            }
        }

        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutex);
            printf("worker %ld destroy\n", pthread_self());
            pool->thread_exit_();
        }

        Task task;
        task = pool->task_q.front();
        pool->task_q.pop();
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&pool->busy_mutex);
        pool->busy_thread++;
        pthread_mutex_unlock(&pool->busy_mutex);

        task.function(task.arg);

        pthread_mutex_lock(&pool->busy_mutex);
        pool->busy_thread--;
        pthread_mutex_unlock(&pool->busy_mutex);
    }

    return nullptr;
}

void* ThreadPool::manager_(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;
    while(!pool->shutdown){
        pthread_mutex_lock(&pool->mutex);
        if(pool->task_q.size() > pool->live_thread &&
            pool->live_thread < pool->max_thread)
        {
            int count = 0;
            for(long_int i = 0;
                count < pool->per_changer_num_ &&
                i < pool->max_thread &&
                pool->live_thread < pool->max_thread;
                i++)
            {
                if(pool->worker_ids[i] == 0){
                    pool->live_thread++;
                    pthread_create(&pool->worker_ids[i], NULL, worker_, pool);
                }
            }
        }
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&pool->mutex);
        pthread_mutex_lock(&pool->busy_mutex);

        if((2 * pool->task_q.size()) < pool->live_thread &&
            pool-> live_thread > pool->min_thread)
        {
            pool->exit_thread += pool->per_changer_num_;
            for(int i = 0; i < pool->per_changer_num_; i++){
                pthread_cond_signal(&pool->cant_empty);
            }
        }

        pthread_mutex_unlock(&pool->busy_mutex);
        pthread_mutex_unlock(&pool->mutex);

        sleep(2);
    }
    printf("manager thread destroy...\n");
    return nullptr;
}