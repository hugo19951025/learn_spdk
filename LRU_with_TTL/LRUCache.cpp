#include <unordered_map>
#include <list>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <iostream>
#include <thread>


class LRUCacheWithTTL {
private:
    struct Node {
        int key;
        int value;
        std::chrono::steady_clock::time_point expire_time;

        Node(int k, int v, std::chrono::seconds ttl) : key(k), value(v), expire_time(std::chrono::steady_clock::now() + ttl) {}
    };

    std::chrono::seconds ttl;
    int capacity;
    std::list<Node> cache;
    std::unordered_map<int, std::list<Node>::iterator> key_map;

    void clean_expire() {
        auto it = cache.begin();
        auto now = std::chrono::steady_clock::now();
        while (it != cache.end()) {
            if (it->expire_time <= now) {
                key_map.erase(it->key);
                it = cache.erase(it);
            } else {
                it++;
            }
        }
    }

    bool is_expired(const Node& node) {
        return node.expire_time <= std::chrono::steady_clock::now();
    }
public:
    LRUCacheWithTTL(int capacity, int ttl = 30) : capacity(capacity), ttl(std::chrono::seconds(ttl)) {

    }

    int get(int key) {
        clean_expire();
        auto it = key_map.find(key);
        if (it == key_map.end()) {
            return -1;
        }

        if (is_expired(*it->second)) {
            cache.erase(it->second);
            key_map.erase(it->second->key);
            return -1;
        }

        cache.splice(cache.begin(), cache, it->second);
        return it->second->value;
    }

    void put(int key, int value) {
        clean_expire();

        auto it = key_map.find(key);
        if (it == key_map.end()) {
            if (cache.size() == capacity) {
                int last_key = cache.back().key;
                key_map.erase(last_key);
                cache.pop_back();
            }
            cache.emplace_front(key, value, ttl);
            key_map[key] = cache.begin();
        } else {
            it->second->value = value;
            it->second->expire_time = std::chrono::steady_clock::now() + ttl;
            cache.splice(cache.begin(), cache, it->second);
        }
    }
};

int main() {
    std::cout << "=== 带TTL的LRU缓存测试 ===" << std::endl;
    
    // 测试简化版本
    LRUCacheWithTTL cache(2, 2); // 容量2，TTL 2秒
    
    cache.put(1, 1);
    cache.put(2, 2);
    std::cout << "get(1): " << cache.get(1) << std::endl; // 返回1
    
    cache.put(3, 3); // 淘汰key 2
    std::cout << "get(2): " << cache.get(2) << std::endl; // 返回-1（被淘汰）
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "After 3 seconds, get(1): " << cache.get(1) << std::endl; // 返回-1（过期）
    return 0;
}