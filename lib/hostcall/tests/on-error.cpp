#include "common.hpp"

int
main()
{
    hsa_init();

    auto consumer = amd_hostcall_create_consumer();

    CHECK(amd_hostcall_on_error(consumer, print_error, nullptr));
    CHECK(amd_hostcall_launch_consumer(consumer));

    if (amd_hostcall_on_error(consumer, print_error, nullptr) !=
        AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE) {
        return __LINE__;
    }

    return 0;
}
