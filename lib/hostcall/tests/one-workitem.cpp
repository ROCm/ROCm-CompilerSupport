#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

void
handler(void *ignored, uint32_t service, uint64_t *payload)
{
    *payload = *payload + 1;
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
    amd_hostcall_register_service(consumer, TEST_SERVICE, handler, nullptr);
    CHECK(amd_hostcall_launch_consumer(consumer));

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

    auto pred = std::bind(check_zero, &header->control);
    if (timeout(pred, 50)) {
        return __LINE__;
    }

    if (payload->slots[0][0] != 43)
        return __LINE__;

    amd_hostcall_destroy_consumer(consumer);
    free(buffer);

    return 0;
}
