#include "common.hpp"

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    ASSERT(hsa_init() == HSA_STATUS_SUCCESS);

    auto consumer = init_consumer(nullptr);

    CHECK(amd_hostcall_on_error(print_error, nullptr));

    ASSERT(amd_hostcall_on_error(print_error, nullptr) ==
           AMD_HOSTCALL_ERROR_INVALID_REQUEST);

    test_passed();
}
