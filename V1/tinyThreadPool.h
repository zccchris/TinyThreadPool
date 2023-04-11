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
                    std::unique_lock<std::mutex> lock(TasksQueue_mutex);
                    condition.wait(lock, [this] { return stop || !TasksQueue.empty(); });
                    if (stop && TasksQueue.empty()) return;
                    auto task = TasksQueue.front();
                    TasksQueue.pop();
                    TasksQueue_mutex.unlock();
                    task();
                }
                });
        }
    }
    ~ThreadPool() {
        stop = true;
        condition.notify_all();
        for (std::thread& worker : WorkersThread)
            worker.join();
    }

    void AddTask(void(*f)(void)) {

        TasksQueue_mutex.lock();
        TasksQueue.emplace(*f);
        condition.notify_one();
        TasksQueue_mutex.unlock();

    }
private:
    std::vector<std::thread> WorkersThread;
    std::queue <std::function<void()>> TasksQueue;
    std::mutex TasksQueue_mutex;
    std::condition_variable condition;
    bool stop;
};