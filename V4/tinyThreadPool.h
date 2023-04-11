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
	//构造函数中，不再创建线程，只确定最小和最大线程数量
	//默认最小线程数0，最大线程数为系统支持的最大线程数
	ThreadPool(size_t MaxThreads = 0, size_t MinThreads = 0)
		: stop(false)
		, Min_ThreadNum(MinThreads)
		, Max_ThreadNum(MaxThreads)
		, Current_ThreadNum(0) {
		if (Max_ThreadNum == 0 || Max_ThreadNum > std::thread::hardware_concurrency()) Max_ThreadNum = std::thread::hardware_concurrency();
	}

	~ThreadPool() {
		std::unique_lock<std::mutex> lock(TasksQueue_mutex);
		stop = true;
		condition.notify_all();

	}
	//当调用这个函数时，会为线程池新增一个线程并detach。
	//新增的线程有自己的销毁机制。
	void addThread() {
		std::thread OneThread(
			[this]() {
				for (;;) {
					std::unique_lock<std::mutex> lock(TasksQueue_mutex);
					if (condition.wait_for(lock, std::chrono::seconds(60)) == std::cv_status::timeout) {
						return;
						Current_ThreadNum--;
					}
					else {
						if (stop) return;
						std::function<void()> task = std::move(TasksQueue.front());
						TasksQueue.pop();
						task();
					}
				}
			}
		);
		OneThread.detach();
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

		std::unique_lock<std::mutex> lock(TasksQueue_mutex);
		if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
		TasksQueue.emplace([task]() { (*task)(); });

		if (Current_ThreadNum == 0 || (TasksQueue.size() > 10 && Current_ThreadNum < Max_ThreadNum)) addThread();
		condition.notify_one();
		return res;
	}

private:
	std::queue<std::function<void()>> TasksQueue;
	std::mutex TasksQueue_mutex;
	std::condition_variable condition;
	bool stop;
	//维护两个值，分别为最大的线程数，和当前维护的线程数。
	int Min_ThreadNum;
	int Max_ThreadNum;
	int Current_ThreadNum;
};