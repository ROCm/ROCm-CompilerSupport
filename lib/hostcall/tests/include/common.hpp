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
push_stack(buffer_t *buffer, uint64_t *top, uint64_t ptr)
{
    header_t *P = get_header(buffer, ptr);

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
push_ready_stack(buffer_t *buffer, uint64_t ptr)
{
    uint64_t *top = &buffer->ready_stack;
    push_stack(buffer, top, ptr);
}

static ulong
inc_ptr_tag(ulong ptr, uint index_size)
{
    // Unit step for the tag.
    ulong inc = 1UL << index_size;
    ptr += inc;
    // When the tag for index 0 wraps, increment the tag.
    return ptr == 0 ? inc : ptr;
}

static void
push_free_stack(buffer_t *buffer, uint64_t ptr)
{
    uint64_t *top = &buffer->free_stack;
    ptr = inc_ptr_tag(ptr, buffer->index_size);
    push_stack(buffer, top, ptr);
}

static void
send_signal(signal_t signal)
{
    hsa_signal_t hs{signal.handle};
    hsa_signal_add_release(hs, 1);
}

static bool
check_value(uint32_t *ptr, uint32_t value)
{
    return __atomic_load_n(ptr, std::memory_order_acquire) == value;
}

static bool
check_zero(uint32_t *ptr)
{
    return check_value(ptr, 0);
}

static bool
ready_flag_is_unset(uint32_t *ptr)
{
    uint32_t value = __atomic_load_n(ptr, std::memory_order_acquire);
    return (get_ready_flag(value) == 0);
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

static void
print_error(void *ignored, int error)
{
    std::cout << amd_hostcall_error_string(error) << std::endl;
}

static void *
init_buffer(int num_packets)
{
    auto unaligned_buffer = create_buffer(num_packets);
    ASSERT(unaligned_buffer);
    auto buffer = realign_buffer(unaligned_buffer);
    CHECK(amd_hostcall_initialize_buffer(buffer, num_packets));

    return buffer;
}

static amd_hostcall_consumer_t *
init_consumer(void *buffer)
{
    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    amd_hostcall_consumer_t *consumer;
    CHECK(amd_hostcall_create_consumer(&consumer));
    ASSERT(consumer);

    CHECK(amd_hostcall_launch_consumer(consumer));

    if (buffer) {
        CHECK(amd_hostcall_register_buffer(consumer, buffer));
    }

    return consumer;
}

#endif // COMMON_HPP
