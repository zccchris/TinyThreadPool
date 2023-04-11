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
	//���캯���У����ٴ����̣߳�ֻȷ����С������߳�����
	//Ĭ����С�߳���0������߳���Ϊϵͳ֧�ֵ�����߳���
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
	//�������������ʱ����Ϊ�̳߳�����һ���̲߳�detach��
	//�������߳����Լ������ٻ��ơ�
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
	//ά������ֵ���ֱ�Ϊ�����߳������͵�ǰά�����߳�����
	int Min_ThreadNum;
	int Max_ThreadNum;
	int Current_ThreadNum;
};