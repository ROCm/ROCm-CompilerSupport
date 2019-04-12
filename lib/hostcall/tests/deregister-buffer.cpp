#include "common.h"

int
main()
{
    if (hsa_init() != HSA_STATUS_SUCCESS)
        return __LINE__;

    auto consumer = amd_hostcall_create_consumer();
    if (!consumer)
        return __LINE__;

    const uint32_t num_packets = 3;
    void *buffer = create_buffer(num_packets);
    if (!buffer)
        return __LINE__;
    void *aligned_buffer = realign_buffer(buffer);

    if (amd_hostcall_deregister_buffer(consumer, buffer) !=
        AMD_HOSTCALL_ERROR_INVALID_REQUEST) {
        return __LINE__;
    }

    amd_hostcall_register_buffer(consumer, buffer);

    if (amd_hostcall_deregister_buffer(consumer, buffer) !=
        AMD_HOSTCALL_SUCCESS) {
        return __LINE__;
    }

    if (amd_hostcall_deregister_buffer(consumer, buffer) !=
        AMD_HOSTCALL_ERROR_INVALID_REQUEST) {
        return __LINE__;
    }

    free(buffer);
    amd_hostcall_destroy_consumer(consumer);

    return 0;
}
