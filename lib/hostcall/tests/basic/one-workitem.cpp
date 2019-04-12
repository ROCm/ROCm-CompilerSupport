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

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    const int num_packets = 1;
    auto buffer = init_buffer(num_packets);
    auto consumer = init_consumer(buffer);
    amd_hostcall_register_service(TEST_SERVICE, handler, nullptr);

    auto hb = reinterpret_cast<buffer_t *>(buffer);
    auto F = pop_free_stack(hb);

    auto header = get_header(hb, F);
    header->control = set_ready_flag(header->control);
    header->service = TEST_SERVICE;
    header->activemask = 1;

    auto payload = get_payload(hb, F);
    payload->slots[0][0] = 42;

    push_ready_stack(hb, F);
    send_signal(hb->doorbell);

    auto pred = std::bind(ready_flag_is_unset, &header->control);
    ASSERT(!timeout(pred, 50));

    ASSERT(payload->slots[0][0] == 43);

    CHECK(amd_hostcall_destroy_consumer(consumer));
    free(buffer);

    test_passed();
}
