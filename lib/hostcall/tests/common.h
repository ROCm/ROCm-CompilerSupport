#ifndef COMMON_H
#define COMMON_H

#include <hostcall_impl.h>
#include <hsa/hsa.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define TEST_SERVICE 23

int debug_mode = 0;
#define WHEN_DEBUG(xxx)                                                        \
    do {                                                                       \
        if (debug_mode) {                                                      \
            xxx;                                                               \
        }                                                                      \
    } while (0)

#define CHECK(xxx)                                                             \
    do {                                                                       \
        amd_hostcall_error_t error = xxx;                                      \
        if (error != AMD_HOSTCALL_SUCCESS) {                                   \
            return __LINE__;                                                   \
        }                                                                      \
    } while (0);

#define runTest(xxx)                                                           \
    do {                                                                       \
        int status = xxx();                                                    \
        if (status != 0)                                                       \
            return status;                                                     \
    } while (0);

static int
parse_options(int argc, char *argv[])
{
    for (int ii = 1; ii != argc; ++ii) {
        char *str = argv[ii];
        if (str[0] != '-')
            return 0;
        if (str[2])
            return 0;
        switch (str[1]) {
        case 'd':
            debug_mode = 1;
            break;
        default:
            return 0;
            break;
        }
    }

    return 1;
}

static int
set_flags(int argc, char *argv[])
{
    debug_mode = 0;

    if (!parse_options(argc, argv)) {
        printf("invalid command-line arguments\n");
        return 0;
    }

    return 1;
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
    memset(buffer, 0, allocated_size);
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

static payload_t *
get_payload(buffer_t *buffer, uint64_t ptr)
{
    return buffer->payloads + get_ptr_index(ptr, buffer->index_size);
}

#endif // COMMON_H
