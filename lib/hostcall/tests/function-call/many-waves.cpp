#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

static void
add_pairs(uint64_t *output, uint64_t *input)
{
    output[0] = input[0] + input[1];
    output[1] = input[2] + input[3];
}

void
producer_func(buffer_t *hb, uint64_t id,
              std::chrono::system_clock::time_point start, uint32_t *done)
{
    // A feeble attempt at starting producers close to each other.
    // TODO: Use std::shared_lock from C++14.
    std::this_thread::sleep_until(start);

    auto F = pop_free_stack(hb);
    auto header = get_header(hb, F);
    header->control = set_ready_flag(header->control);
    header->service = SERVICE_FUNCTION_CALL;
    header->activemask = 1;

    auto payload = get_payload(hb, F);
    payload->slots[0][0] = (uint64_t)add_pairs;
    payload->slots[0][1] = 91;
    payload->slots[0][2] = 5;
    payload->slots[0][3] = 23;
    payload->slots[0][4] = 17;

    push_ready_stack(hb, F);
    send_signal(hb->doorbell);

    while (!ready_flag_is_unset(&header->control)) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    ASSERT(payload->slots[0][0] == 96);
    ASSERT(payload->slots[0][1] == 40);

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
