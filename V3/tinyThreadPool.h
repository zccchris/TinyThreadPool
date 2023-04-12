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
                    //�Թ������еĴ��������ٽ�������
                    std::unique_lock<std::mutex> lock(TasksQueue_mutex);
                    condition.wait(lock, [this] { return stop || !TasksQueue.empty(); });
                    //��ֹ����
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


    //������һ��task�󣬽�taskͨ��bind()��װ��һ���ɵ��ö����ٽ�����ɵ��ö�����ж��η�װ����packaged_task��������������󣬿��Խ��ɵ��ö����ִ�кͷ��ؽ��з��롣��װ����һ������ָ��ָ�����packaged_task���󣬲�����ӵ����������future��
    //�����packaged_task�����ָ����ӣ��ȴ����̳߳ص��ã����ض����future����ִ�к����䷵��ֵ��

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