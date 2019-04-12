#include "common.hpp"

#include <atomic>
#include <iostream>
#include <set>
#include <thread>

typedef struct {
    std::set<uint64_t> elements;
} test_data_t;

int
handler(void *state, uint32_t service, uint64_t *payload)
{
    auto *td = reinterpret_cast<test_data_t *>(state);
    td->elements.insert(*payload);
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

    test_data_t td;
    amd_hostcall_register_service(TEST_SERVICE, handler, &td);

    auto hb = reinterpret_cast<buffer_t *>(buffer);
    auto F = pop_free_stack(hb);

    auto header = get_header(hb, F);
    header->control = set_ready_flag(header->control);
    header->service = TEST_SERVICE;
    header->activemask = 0x8421;

    auto payload = get_payload(hb, F);
    payload->slots[0][0] = 42;
    payload->slots[5][0] = 43;
    payload->slots[10][0] = 44;
    payload->slots[15][0] = 45;

    push_ready_stack(hb, F);
    send_signal(hb->doorbell);

    auto pred = std::bind(ready_flag_is_unset, &header->control);
    ASSERT(!timeout(pred, 50));

    ASSERT(td.elements.size() == 4);
    ASSERT(td.elements.count(42) == 1);
    ASSERT(td.elements.count(43) == 1);
    ASSERT(td.elements.count(44) == 1);
    ASSERT(td.elements.count(45) == 1);

    CHECK(amd_hostcall_destroy_consumer(consumer));
    free(buffer);

    test_passed();
}
