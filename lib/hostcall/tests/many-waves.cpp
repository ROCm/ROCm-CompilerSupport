#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

void
handler(void *ignored, uint32_t service, uint64_t *payload)
{
    *payload = *payload + 1;
}

void
producer_func(buffer_t *hb, bool *status, uint64_t id,
              std::chrono::system_clock::time_point start, uint32_t *done)
{
    // A feeble attempt at starting producers close to each other.
    // TODO: Use std::shared_lock from C++14.
    std::this_thread::sleep_until(start);

    auto F = pop_free_stack(hb);
    auto header = get_header(hb, F);
    header->control = set_ready_flag(header->control);
    header->service = TEST_SERVICE;
    header->activemask = 1;

    auto payload = get_payload(hb, F);
    payload->slots[0][0] = id;

    push_ready_stack(hb, F);
    send_signal(hb->doorbell);

    while (__atomic_load_n(&header->control, std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    *status = (payload->slots[0][0] == id + 1);
    __atomic_fetch_add(done, 1, std::memory_order_relaxed);
}

int
main(int argc, char *argv[])
{
    set_flags(argc, argv);
    if (debug_mode)
        amd_hostcall_enable_debug();

    if (hsa_init() != HSA_STATUS_SUCCESS)
        return __LINE__;

    const int num_threads = 1000;
    const int num_packets = num_threads;

    auto unaligned_buffer = create_buffer(num_packets);
    if (!unaligned_buffer)
        return __LINE__;
    auto buffer = realign_buffer(unaligned_buffer);
    CHECK(amd_hostcall_initialize_buffer(buffer, num_packets));

    auto consumer = amd_hostcall_create_consumer();
    if (!consumer)
        return __LINE__;

    amd_hostcall_register_buffer(consumer, buffer);
    amd_hostcall_register_service(consumer, TEST_SERVICE, handler, nullptr);
    CHECK(amd_hostcall_launch_consumer(consumer));

    auto hb = reinterpret_cast<buffer_t *>(buffer);

    bool status[num_threads];
    std::thread producers[num_threads];
    uint32_t done = 0;
    std::chrono::system_clock::time_point start =
        std::chrono::system_clock::now() + std::chrono::milliseconds(50);

    for (int i = 0; i != num_threads; ++i) {
        producers[i] =
            std::thread(producer_func, hb, &status[i], i, start, &done);
    }

    auto pred = std::bind(check_zero, &done);
    if (timeout(pred, 100))
        return __LINE__;

    for (int i = 0; i != num_threads; ++i) {
        producers[i].join();
    }

    amd_hostcall_destroy_consumer(consumer);
    free(buffer);

    for (int i = 0; i != num_threads; ++i) {
        if (!status[i])
            return __LINE__;
    }

    return 0;
}
