#pragma once

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <linux/futex.h>
#include <stdexcept>
#include <sys/syscall.h>
#include <unistd.h>

class FutexMutex {
public:
    FutexMutex() = default;

    FutexMutex(const FutexMutex&) = delete;
    FutexMutex& operator=(const FutexMutex&) = delete;

    void lock() {
        int expected = kUnlocked;
        if (state_.compare_exchange_strong(
                expected,
                kLockedNoWaiters,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return;
        }

        lock_slow(expected);
    }

    bool try_lock() {
        int expected = kUnlocked;
        return state_.compare_exchange_strong(
            expected,
            kLockedNoWaiters,
            std::memory_order_acquire,
            std::memory_order_relaxed);
    }

    void unlock() {
        const int previous = state_.fetch_sub(1, std::memory_order_release);
        if (previous == kLockedNoWaiters) {
            return;
        }

        state_.store(kUnlocked, std::memory_order_release);
        futex_wake_one();
    }

private:
    static_assert(sizeof(std::atomic<int>) == sizeof(int));
    static_assert(alignof(std::atomic<int>) >= alignof(int));

    static constexpr int kUnlocked = 0;
    static constexpr int kLockedNoWaiters = 1;
    static constexpr int kLockedWithWaiters = 2;

    void lock_slow(int observed) {
        for (;;) {
            if (observed != kLockedWithWaiters) {
                observed = state_.exchange(kLockedWithWaiters, std::memory_order_acquire);
            }

            while (observed != kUnlocked) {
                const int rc = syscall(
                    SYS_futex,
                    reinterpret_cast<int*>(&state_),
                    FUTEX_WAIT_PRIVATE,
                    kLockedWithWaiters,
                    nullptr,
                    nullptr,
                    0);

                if (rc == -1 && errno != EAGAIN && errno != EINTR) {
                    throw std::runtime_error("futex wait failed");
                }

                observed = state_.exchange(kLockedWithWaiters, std::memory_order_acquire);
            }

            if (observed == kUnlocked) {
                return;
            }
        }
    }

    void futex_wake_one() noexcept {
        syscall(
            SYS_futex,
            reinterpret_cast<int*>(&state_),
            FUTEX_WAKE_PRIVATE,
            1,
            nullptr,
            nullptr,
            0);
    }

    alignas(4) std::atomic<int> state_{kUnlocked};
};
