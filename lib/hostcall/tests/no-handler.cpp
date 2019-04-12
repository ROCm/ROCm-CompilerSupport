#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

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
    CHECK(amd_hostcall_on_error(consumer, print_error, nullptr));
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

    // A simple timeout to ensure that the test never hangs.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // The test should never reach here, since the consumer should
    // cause the program to exit because of the missing service
    // handler. See CMakeLists.txt for more information.
    return 0;
}
