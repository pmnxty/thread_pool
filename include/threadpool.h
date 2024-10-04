#include <pthread.h>
#include <queue>
using callback = void (*)(void*);
using long_int = long unsigned int;

typedef struct Task {
    Task();
    Task(callback func, void* a);

    callback function;
    void* arg;
} Task;

class ThreadPool {
   public:
    ThreadPool(int min, int max);
    ~ThreadPool();
    void add_task(Task task);
    long_int get_busy();
    long_int get_live();

   private:
    static void* manager_(void*);
    static void* worker_(void*);
    void thread_exit_();

   private:
    std::queue<Task> task_q;
    pthread_t manager_id;
    pthread_t* worker_ids;
    pthread_mutex_t mutex;
    pthread_mutex_t busy_mutex;
    pthread_cond_t cant_empty;
    long_int max_thread;
    long_int min_thread;
    long_int busy_thread;
    long_int live_thread;
    long_int exit_thread;
    bool shutdown;
    const int per_changer_num_ = 2;
};
