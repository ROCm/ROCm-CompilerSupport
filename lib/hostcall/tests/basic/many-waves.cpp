#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

int
handler(void *ignored, uint32_t service, uint64_t *payload)
{
    *payload = *payload + 1;
    return 0;
}

void
producer_func(buffer_t *hb, uint64_t id,
              std::chrono::system_clock::time_point start, uint32_t *done)
{
    // A feeble attempt at starting producers close to each other.
    // TODO: Use std::shared_lock from C++14.
    std::this_thread::sleep_until(start);

    for (int i = 0; i != 10; ++i) {
        auto F = pop_free_stack(hb);
        auto header = get_header(hb, F);
        header->control = set_ready_flag(header->control);
        header->service = TEST_SERVICE;
        header->activemask = 1;

        auto payload = get_payload(hb, F);
        payload->slots[0][0] = id * i;

        push_ready_stack(hb, F);
        send_signal(hb->doorbell);

        while (!ready_flag_is_unset(&header->control)) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        ASSERT(payload->slots[0][0] == id * i + 1);
        push_free_stack(hb, F);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    __atomic_fetch_add(done, 1, std::memory_order_relaxed);
}

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    const int num_threads = 1000;
    const int num_packets = num_threads;

    auto buffer = init_buffer(num_packets);
    auto consumer = init_consumer(buffer);
    amd_hostcall_register_service(TEST_SERVICE, handler, nullptr);

    auto hb = reinterpret_cast<buffer_t *>(buffer);

    std::thread producers[num_threads];
    uint32_t done = 0;
    std::chrono::system_clock::time_point start =
        std::chrono::system_clock::now() + std::chrono::milliseconds(50);

    for (int i = 0; i != num_threads; ++i) {
        producers[i] = std::thread(producer_func, hb, i, start, &done);
    }

    auto pred = std::bind(check_value, &done, num_threads);
    ASSERT(!timeout(pred, 500));

    for (int i = 0; i != num_threads; ++i) {
        producers[i].join();
    }

    CHECK(amd_hostcall_destroy_consumer(consumer));
    free(buffer);

    test_passed();
}
