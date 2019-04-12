#include "common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    const int num_packets = 1;
    auto buffer = init_buffer(num_packets);
    auto consumer = init_consumer(buffer);

    CHECK(amd_hostcall_on_error(print_error, nullptr));

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

    // This is a negative test. See CMakeLists.txt for more information.
    test_failed("consumer failed to abort on missing service handler");
}
