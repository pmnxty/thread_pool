
#include <iostream>
#include <string>
#include <unistd.h>
#include "threadpool.h"

void function(void* arg){
    int num = *(int*) arg;
    printf("thread %ld output data %d...\n", pthread_self(), num);
    sleep(1);
}

int main(){
    {
        ThreadPool pool(3, 10);
        for(int i = 0; i < 100; i++){
            Task task;
            int* num = new int;
            *num = i;
            task.arg = num;
            task.function = function;
            pool.add_task(task);
        }

        sleep(15);
    }

    system("pause");

    return 0;
}