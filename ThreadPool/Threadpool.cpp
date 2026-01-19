#include <iostream>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <stdexcept>
#include <functional>


class Threadpool {
public:
Threadpool(int thread_count) : stop(false) {
    for (int i = 0; i < thread_count; i++) {
        workers.emplace_back(
            [this]() {
                while (1) {
                    std::function<void(void)> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);

                        condition.wait(lock, [this]() {
                            return stop || !this->tasks.empty();
                        });

                        if (stop && this->tasks.empty()) {
                            return;
                        }

                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            }
        );
    }
}


~Threadpool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }
}


template<typename Func, typename... Args>
std::future<std::invoke_result_t<Func, Args...>> add_task(Func&& func, Args&&... args) {
    using returntype = std::invoke_result_t<Func, Args...>;

    auto bound_func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);

    auto task = std::make_shared<std::packaged_task<returntype(void)>>(bound_func);


    auto result = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (stop) {
            throw std::runtime_error("error");
        }
        tasks.push( [task](){ (*task)(); } );
    }

    condition.notify_one();
    return result;
}
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void(void)>> tasks;
    bool stop;
    std::mutex queue_mutex;
    std::condition_variable condition;
};


int main() {
    Threadpool pool(4);
    std::vector<std::future<int>> results;


    for (int i = 0; i < 8; i++) {
        results.emplace_back(
            pool.add_task(
                [i]() {
                    std::cout << "Thread id " << std::this_thread::get_id() << " run in task " << i << "." << std::endl;
                    return i * i; 
                }
            )
        );
    }


    std::this_thread::sleep_for(std::chrono::seconds(20));
    for (auto& result : results) {
        std::cout << result.get() << std::endl;
    }
    return 0;
}