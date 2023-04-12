#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class ThreadPool {
public:
    ThreadPool(int ThreadsNum) : stop(false) {
        for (int i = 0; i < ThreadsNum; ++i) {
            WorkersThread.emplace_back([this] {
                for (;;) {
                    //对工作队列的处理属于临界区代码
                    std::unique_lock<std::mutex> lock(TasksQueue_mutex);
                    condition.wait(lock, [this] { return stop || !TasksQueue.empty(); });
                    //终止条件
                    if (stop && TasksQueue.empty()) return;
                    std::function<void()> task = std::move(TasksQueue.front());
                    TasksQueue.pop();
                    task();
                }
                });
        }
    }
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(TasksQueue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& aThread : WorkersThread)
            aThread.join();
    }


    //传进来一个task后，将task通过bind()封装成一个可调用对象，再将这个可调用对象进行二次封装成用packaged_task对象，有了这个对象，可以将可调用对象的执行和返回进行分离。封装后建立一个智能指针指向这个packaged_task对象，并可以拥有这个对象的future。
    //将这个packaged_task对象的指针入队，等待被线程池调用，返回对象的future，待执行后获得其返回值。

    template<class F, class... Args>
    auto GetTask(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(TasksQueue_mutex);
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            TasksQueue.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

private:
    std::vector<std::thread> WorkersThread;
    std::queue<std::function<void()>> TasksQueue;
    std::mutex TasksQueue_mutex;
    std::condition_variable condition;
    bool stop;
};