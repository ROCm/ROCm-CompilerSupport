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

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    const int num_packets = 1;
    auto buffer = init_buffer(num_packets);
    auto consumer = init_consumer(buffer);

    auto hb = reinterpret_cast<buffer_t *>(buffer);
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

    auto pred = std::bind(ready_flag_is_unset, &header->control);
    ASSERT(!timeout(pred, 50));

    ASSERT(payload->slots[0][0] == 96);
    ASSERT(payload->slots[0][1] == 40);

    CHECK(amd_hostcall_destroy_consumer(consumer));
    free(buffer);

    test_passed();
}
