#ifndef COMMON_HPP
#define COMMON_HPP

#include "common.h"

#include <atomic>
#include <functional>
#include <iostream>
#include <thread>

static uint64_t
pop_free_stack(buffer_t *buffer)
{
    uint64_t *top = &buffer->free_stack;
    uint64_t F = __atomic_load_n(top, std::memory_order_acquire);

    while (true) {
        header_t *header = get_header(buffer, F);
        uint64_t N = header->next;

        if (__atomic_compare_exchange_n(top, &F, N,
                                        /* weak = */ false,
                                        std::memory_order_acquire,
                                        std::memory_order_relaxed)) {
            return F;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

static void
push_ready_stack(buffer_t *buffer, uint64_t ptr)
{
    header_t *P = get_header(buffer, ptr);
    uint64_t *top = &buffer->ready_stack;

    uint64_t F = __atomic_load_n(top, std::memory_order_relaxed);
    while (true) {
        P->next = F;
        if (__atomic_compare_exchange_n(top, &F, ptr, false,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

static void
send_signal(signal_t signal)
{
    hsa_signal_t hs{signal.handle};
    hsa_signal_add_release(hs, 1);
}

static void
print_error(amd_hostcall_error_t error, void *ignored)
{
    std::cout << amd_hostcall_error_string(error) << std::endl;
}

static bool
check_zero(uint32_t *ptr)
{
    if (__atomic_load_n(ptr, std::memory_order_acquire) == 0) {
        return true;
    }
    return false;
}

static bool
timeout(const std::function<bool()> &pred, uint millisecs)
{
    using std::chrono::system_clock;
    system_clock::time_point start = system_clock::now();
    while (true) {
        if (pred())
            return false;
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        system_clock::time_point now = system_clock::now();
        if (now - start > std::chrono::milliseconds(millisecs)) {
            return true;
        }
    }
}

#endif // COMMON_HPP
