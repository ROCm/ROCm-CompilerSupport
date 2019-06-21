#ifndef COMMON_H
#define COMMON_H

#include <hostcall_impl.h>
#include <hsa/hsa.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SERVICE 23
#define SERVICE_FUNCTION_CALL 1

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define test_passed()                                                          \
    printf("%sPASSED!%s\n", KGRN, KNRM);                                       \
    exit(0);

#define test_failed(...)                                                       \
    printf("%serror: ", KRED);                                                 \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
    printf("error: TEST FAILED\n%s", KNRM);                                    \
    abort();

#define CHECK(api_call)                                                        \
    do {                                                                       \
        amd_hostcall_error_t error = api_call;                                 \
        if (error != AMD_HOSTCALL_SUCCESS) {                                   \
            printf("%serror: '%s'(%d) from %s at %s:%d%s\n", KRED,             \
                   amd_hostcall_error_string(error), error, #api_call,         \
                   __FILE__, __LINE__, KNRM);                                  \
            test_failed("API returned error code.");                           \
        }                                                                      \
    } while (0);

#define ASSERT(expr)                                                           \
    if ((expr)) {                                                              \
    } else {                                                                   \
        test_failed("%sassertion %s at %s:%d%s \n", KRED, #expr, __FILE__,     \
                    __LINE__, KNRM);                                           \
    }

static int
set_flags(int argc, char *argv[])
{
    for (int ii = 1; ii != argc; ++ii) {
        char *str = argv[ii];
        if (str[0] != '-')
            return __LINE__;
        if (str[2])
            return __LINE__;
        switch (str[1]) {
        case 'd':
            amd_hostcall_enable_debug();
            break;
        default:
            return __LINE__;
            break;
        }
    }

    return 0;
}

static uintptr_t
align_to(uintptr_t value, uint32_t alignment)
{
    if (value % alignment == 0)
        return value;
    return value - (value % alignment) + alignment;
}

static void *
create_buffer(uint32_t num_packets)
{
    const uint32_t alignment = amd_hostcall_get_buffer_alignment();
    const size_t buffer_size = amd_hostcall_get_buffer_size(num_packets);
    const size_t allocated_size = buffer_size + alignment;
    void *buffer = malloc(allocated_size);
    if (!buffer)
        return 0;
    memset(buffer, ~0, allocated_size);
    return buffer;
}

static void *
realign_buffer(void *buffer)
{
    if (!buffer)
        return 0;
    const uint32_t alignment = amd_hostcall_get_buffer_alignment();
    return (void *)align_to((uintptr_t)buffer, alignment);
}

static uint64_t
get_ptr_index(uint64_t ptr, uint32_t index_size)
{
    return ptr & (((uint64_t)1 << index_size) - 1);
}

static header_t *
get_header(buffer_t *buffer, uint64_t ptr)
{
    return buffer->headers + get_ptr_index(ptr, buffer->index_size);
}

static uint32_t
set_ready_flag(uint32_t control)
{
    return control | 1;
}

static uint32_t
get_ready_flag(uint32_t control)
{
    return control & 1;
}

static payload_t *
get_payload(buffer_t *buffer, uint64_t ptr)
{
    return buffer->payloads + get_ptr_index(ptr, buffer->index_size);
}

#endif // COMMON_H
