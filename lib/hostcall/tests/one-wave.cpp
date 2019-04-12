#include "common.hpp"

#include <atomic>
#include <iostream>
#include <set>
#include <thread>

typedef struct {
    std::set<uint64_t> elements;
} test_data_t;

void
handler(void *cbdata, uint32_t service, uint64_t *payload)
{
    auto *td = reinterpret_cast<test_data_t *>(cbdata);
    td->elements.insert(*payload);
}

int
main(int argc, char *argv[])
{
    set_flags(argc, argv);
    if (debug_mode)
        amd_hostcall_enable_debug();

    if (hsa_init() != HSA_STATUS_SUCCESS)
        return __LINE__;

    const int num_packets = 1;
    auto unaligned_buffer = create_buffer(num_packets);
    if (!unaligned_buffer)
        return __LINE__;
    auto buffer = realign_buffer(unaligned_buffer);
    CHECK(amd_hostcall_initialize_buffer(buffer, num_packets));

    auto consumer = amd_hostcall_create_consumer();
    if (!consumer)
        return __LINE__;

    amd_hostcall_register_buffer(consumer, buffer);

    test_data_t td;
    amd_hostcall_register_service(consumer, TEST_SERVICE, handler, &td);
    CHECK(amd_hostcall_launch_consumer(consumer));

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

    auto pred = std::bind(check_zero, &header->control);
    if (timeout(pred, 50)) {
        return __LINE__;
    }

    if (td.elements.size() != 4)
        return __LINE__;
    if (td.elements.count(42) != 1)
        return __LINE__;
    if (td.elements.count(43) != 1)
        return __LINE__;
    if (td.elements.count(44) != 1)
        return __LINE__;
    if (td.elements.count(45) != 1)
        return __LINE__;

    amd_hostcall_destroy_consumer(consumer);
    free(buffer);

    return 0;
}
