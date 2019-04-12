#include "common.hpp"

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    auto consumer = init_consumer(nullptr);

    const uint32_t num_packets = 3;
    auto buffer = init_buffer(num_packets);

    ASSERT(amd_hostcall_deregister_buffer(consumer, buffer) ==
           AMD_HOSTCALL_ERROR_INVALID_REQUEST);

    CHECK(amd_hostcall_register_buffer(consumer, buffer));

    CHECK(amd_hostcall_deregister_buffer(consumer, buffer));

    ASSERT(amd_hostcall_deregister_buffer(consumer, buffer) ==
           AMD_HOSTCALL_ERROR_INVALID_REQUEST);

    free(buffer);
    CHECK(amd_hostcall_destroy_consumer(consumer));

    test_passed();
}
