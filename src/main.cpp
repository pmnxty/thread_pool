#include <unistd.h>

#include <iostream>
#include <string>

#include "threadpool.h"

void func(void* arg) {
    int num = *(int*)arg;
    std::printf("thread %lld output number %d \n", pthread_self(), num);
    sleep(1);
}

int main() {
    {
        ThreadPool pool(3, 10, 100);
        for (int i = 0; i < 100; i++) {
            int* num = new int();
            *num = i * 10;
            pool.pool_add_task(func, num);
        }

        sleep(25);
    }

    std::system("pause");
    return 0;
}