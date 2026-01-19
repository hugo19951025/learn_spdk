#include <iostream>


class Singleton {
private:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator= (const Singleton&) = delete;
public:
    static Singleton& getInstance() {
        static Singleton instance;
        return instance;
    }
};

class SingletonEager {
private:
    static SingletonEager instance; 
    SingletonEager() = default;
    ~SingletonEager() = default;
    SingletonEager(const SingletonEager&) = delete;
    SingletonEager& operator= (const SingletonEager&) = delete;
public:
    static SingletonEager& getInstance() {
        return instance;
    }
};
SingletonEager SingletonEager::instance;

int main() {
    Singleton& s1 = Singleton::getInstance();  // 这里创建
    Singleton& s2 = Singleton::getInstance();

    SingletonEager& s3 = SingletonEager::getInstance();
    SingletonEager& s4 = SingletonEager::getInstance();
}