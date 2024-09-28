#include <iostream>
#include <unistd.h>

#include "threadpool.h"

void func(void* arg) {
    int num = *(int*)arg;
    std::printf("thread %d print number %d\n", pthread_self(), num);
    sleep(1);
}

int main() {
    ThreadPool* pool = create_thread_pool(3, 10, 100);
    for (int i = 0; i < 100; i++) {
        int* num = new int();
        *num = i * 100;
        thread_pool_add_task(pool, func, num);
    }

    sleep(20);

    destroy_thread_pool(pool);
    std::system("pause");
    return 0;
}