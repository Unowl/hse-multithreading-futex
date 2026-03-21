#include "futex_mutex.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

namespace {

void test_try_lock_and_unlock() {
    FutexMutex mutex;

    assert(mutex.try_lock());
    assert(!mutex.try_lock());
    mutex.unlock();
    assert(mutex.try_lock());
    mutex.unlock();
}

void test_lock_guards_shared_counter() {
    FutexMutex mutex;
    constexpr int kThreads = 8;
    constexpr int kIterations = 20'000;
    int counter = 0;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < kIterations; ++j) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    assert(counter == kThreads * kIterations);
}

void test_waiter_blocks_until_unlock() {
    using namespace std::chrono_literals;

    FutexMutex mutex;
    std::atomic<bool> started{false};
    std::atomic<bool> acquired{false};

    mutex.lock();
    std::thread waiter([&] {
        started.store(true, std::memory_order_release);
        mutex.lock();
        acquired.store(true, std::memory_order_release);
        mutex.unlock();
    });

    while (!started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(30ms);
    assert(!acquired.load(std::memory_order_acquire));

    mutex.unlock();
    waiter.join();

    assert(acquired.load(std::memory_order_acquire));
}

void test_multiple_waiters_make_progress() {
    FutexMutex mutex;
    constexpr int kThreads = 6;
    std::atomic<int> entered{0};

    mutex.lock();

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            mutex.lock();
            entered.fetch_add(1, std::memory_order_relaxed);
            mutex.unlock();
        });
    }

    mutex.unlock();

    for (auto& thread : threads) {
        thread.join();
    }

    assert(entered.load(std::memory_order_relaxed) == kThreads);
}

}  // namespace

int main() {
    try {
        test_try_lock_and_unlock();
        test_lock_guards_shared_counter();
        test_waiter_blocks_until_unlock();
        test_multiple_waiters_make_progress();
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << '\n';
        return 1;
    }

    std::cout << "All futex mutex tests passed\n";
    return 0;
}
