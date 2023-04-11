#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    ThreadPool(int ThreadsNum) : stop(false) {
        for (int i = 0; i < ThreadsNum; ++i) {
            WorkersThread.emplace_back([this] {
                for (;;) {
                    //改进点2，用unique_lock实现对TasksQueue_mutex的管理
                    std::unique_lock<std::mutex> lock(TasksQueue_mutex);
                    condition.wait(lock, [this] { return stop || !TasksQueue.empty(); });
                    if (stop && TasksQueue.empty()) return;
                    auto task = TasksQueue.front();
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

    template<class F, class... Args>
    void GetTask(F&& f, Args&&... args) {
        auto task = bind(f, std::forward<Args>(args));
        std::unique_lock<std::mutex> lock(TasksQueue_mutex);
        //改进点1，用lamda表达式捕获task，将该lamda表达式作为可调用对象入队
        TasksQueue.emplace([task]() { (task)(); });
        condition.notify_all();
    }
private:
    std::vector<std::thread> WorkersThread;
    std::queue <std::function<void()>> TasksQueue;
    std::mutex TasksQueue_mutex;
    std::condition_variable condition;
    bool stop;
};