#include <amd_hostcall.h>

int
main(int argc, char *argv[])
{
    amd_hostcall_consumer_t *consumer = amd_hostcall_create_consumer();
    if (consumer)
        return __LINE__;

    return 0;
}
