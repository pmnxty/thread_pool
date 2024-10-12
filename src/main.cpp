#include "threadpool.h"
#include <iostream>
#include <chrono>
#include <vector>

int func(int val){
    printf("thread %lld output a value %d\n", std::this_thread::get_id(), val);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return val * 10;
}

int main(){
    {
        ThreadPool pool(3, 10);
        std::vector<decltype(pool.commit(func, 10000))> vec;
        for(int i = 0; i < 100; i++){
            vec.emplace_back(pool.commit(func, i));
        }

        for(auto it = vec.begin(); it != vec.end(); it++){
            int tmp = (*it).get();
            std::cout << tmp << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}