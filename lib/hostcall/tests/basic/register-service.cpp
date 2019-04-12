#include "common.hpp"

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

    auto consumer = init_consumer(nullptr);

    CHECK(amd_hostcall_register_service(TEST_SERVICE, handler, nullptr));

    ASSERT(amd_hostcall_register_service(TEST_SERVICE, handler, nullptr) ==
           AMD_HOSTCALL_ERROR_INVALID_REQUEST);

    test_passed();
}
