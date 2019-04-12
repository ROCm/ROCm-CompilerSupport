#include "common.h"

int
no_errors()
{
    amd_hostcall_consumer_t *consumer = amd_hostcall_create_consumer();
    if (!consumer)
        return __LINE__;

    const uint32_t num_packets = 4;
    void *buffer = create_buffer(num_packets);
    if (!buffer)
        return __LINE__;
    void *aligned_buffer = realign_buffer(buffer);

    CHECK(amd_hostcall_initialize_buffer(aligned_buffer, num_packets));

    amd_hostcall_register_buffer(consumer, aligned_buffer);

    CHECK(amd_hostcall_launch_consumer(consumer));

    amd_hostcall_destroy_consumer(consumer);
    free(buffer);
    return 0;
}

int
main(int argc, char *argv[])
{
    set_flags(argc, argv);
    if (debug_mode)
        amd_hostcall_enable_debug();

    if (hsa_init() != HSA_STATUS_SUCCESS)
        return __LINE__;
    runTest(no_errors);

    return 0;
}
