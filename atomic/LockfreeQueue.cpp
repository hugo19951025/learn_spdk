#include <atomic>
#include <memory>
#include <iostream>
#include <optional>


//test
#include <vector>
#include <thread>
#include <cassert>


template<typename T>
class LockfreeQueue {
private:
    struct Node {
        T data;
        std::atomic<std::shared_ptr<Node>> next;
        
        Node(T val) : data(std::move(val)), next(nullptr) {}
        ~Node() = default;
    };
    
    std::atomic<std::shared_ptr<Node>> head;
    std::atomic<std::shared_ptr<Node>> tail;
    
public:
    LockfreeQueue() {
        // 创建dummy节点
        auto dummy = std::make_shared<Node>(T{});
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }
    
    ~LockfreeQueue() {
        // 清空队列
        while (dequeue().has_value()) {}
    }
    
    void enqueue(T value) {
        auto new_node = std::make_shared<Node>(std::move(value));
        
        while (true) {
            auto tail_ptr = tail.load(std::memory_order_acquire);
            auto next_ptr = tail_ptr->next.load(std::memory_order_acquire);
            
            // 检查tail是否仍然是队尾
            if (tail_ptr == tail.load(std::memory_order_relaxed)) {
                if (next_ptr == nullptr) {
                    // 尝试链接新节点
                    if (tail_ptr->next.compare_exchange_weak(
                        next_ptr, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                        
                        // 尝试移动tail指针
                        tail.compare_exchange_weak(
                            tail_ptr, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        return;
                    }
                } else {
                    // 帮助移动tail指针
                    tail.compare_exchange_weak(
                        tail_ptr, next_ptr,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }
    
    std::optional<T> dequeue() {
        while (true) {
            auto head_ptr = head.load(std::memory_order_acquire);
            auto tail_ptr = tail.load(std::memory_order_acquire);
            auto next_ptr = head_ptr->next.load(std::memory_order_acquire);
            
            // 检查队列是否为空
            if (head_ptr == head.load(std::memory_order_relaxed)) {
                if (head_ptr == tail_ptr) {
                    if (next_ptr == nullptr) {
                        return std::nullopt;  // 队列为空
                    }
                    // tail落后，帮助移动
                    tail.compare_exchange_weak(
                        tail_ptr, next_ptr,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    // 队列非空，尝试出队
                    if (next_ptr) {
                        T value = std::move(next_ptr->data);
                        if (head.compare_exchange_weak(
                            head_ptr, next_ptr,
                            std::memory_order_release,
                            std::memory_order_relaxed)) {
                            // 成功出队，head_ptr（dummy节点）会自动被shared_ptr释放
                            return value;
                        }
                    }
                }
            }
        }
    }
    
    bool empty() const {
        auto head_ptr = head.load(std::memory_order_acquire);
        return head_ptr == tail.load(std::memory_order_acquire) &&
               head_ptr->next.load(std::memory_order_acquire) == nullptr;
    }
};


void test_queue() {
    LockfreeQueue<int> queue;
    
    // 测试基本功能
    queue.enqueue(1);
    queue.enqueue(2);
    queue.enqueue(3);
    
    // assert(!queue.empty());
    
    auto val1 = queue.dequeue();
    if (val1 && *val1 == 1) {
        std::cout << "Test 1 passed: " << *val1 << std::endl;
    }
    
    auto val2 = queue.dequeue();
    if (val2 && *val2 == 2) {
        std::cout << "Test 2 passed: " << *val2 << std::endl;
    }
    
    auto val3 = queue.dequeue();
    if (val3 && *val3 == 3) {
        std::cout << "Test 3 passed: " << *val3 << std::endl;
    }
    
    auto val4 = queue.dequeue();  // 应该返回空
    if (!val4) {
        std::cout << "Test 4 passed: queue empty" << std::endl;
    }
    
    assert(queue.empty());
    
    // 并发测试
    std::cout << "\nStarting concurrent test..." << std::endl;
    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    const int num_producers = 5;
    const int num_consumers = 5;
    const int items_per_producer = 200;
    
    // 消费者线程
    for (int i = 0; i < num_consumers; ++i) {
        consumer_threads.emplace_back([&queue, &consumed]() {
            int local_consumed = 0;
            while (local_consumed < items_per_producer * num_producers / num_consumers) {
                if (auto val = queue.dequeue()) {
                    ++local_consumed;
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // 生产者线程
    for (int i = 0; i < num_producers; ++i) {
        producer_threads.emplace_back([&queue, i, &produced, items_per_producer]() {
            for (int j = 0; j < items_per_producer; ++j) {
                queue.enqueue(i * items_per_producer + j);
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // 等待所有生产者完成
    for (auto& t : producer_threads) t.join();
    
    // 等待所有消费者完成
    for (auto& t : consumer_threads) t.join();
    
    std::cout << "Produced: " << produced.load() 
              << ", Consumed: " << consumed.load() 
              << ", Final queue empty: " << (queue.empty() ? "yes" : "no") 
              << std::endl;
}

int main() {
    test_queue();
    return 0;
}