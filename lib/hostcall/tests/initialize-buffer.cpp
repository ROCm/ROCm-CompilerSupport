#include "common.h"

#include <iostream>

int
null_pointer()
{
    if (amd_hostcall_initialize_buffer(nullptr, 0) !=
        AMD_HOSTCALL_ERROR_NULLPTR) {
        return __LINE__;
    }

    return 0;
}

int
not_aligned()
{
    uint32_t ignored = -1;
    void *misaligned_buffer = (void *)1;

    if (amd_hostcall_initialize_buffer(misaligned_buffer, ignored) !=
        AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT) {
        return __LINE__;
    }

    return 0;
}

int
no_errors()
{
    const uint32_t num_packets = 42;

    void *buffer = create_buffer(num_packets);
    if (!buffer)
        return __LINE__;
    void *aligned_buffer = realign_buffer(buffer);

    CHECK(amd_hostcall_initialize_buffer(aligned_buffer, num_packets));
    buffer_t *hb = (buffer_t *)aligned_buffer;

    if ((uintptr_t)hb->payloads % alignof(payload_t) != 0)
        return __LINE__;

    if ((uintptr_t)hb->headers % alignof(header_t) != 0)
        return __LINE__;

    if (hb->ready_stack != 0)
        return __LINE__;

    if (hb->free_stack == 0)
        return __LINE__;

    uint64_t iter = hb->free_stack;
    uint32_t count = 0;
    while (iter) {
        ++count;
        auto header = get_header(hb, iter);
        if (header->control != 0)
            return __LINE__;
        iter = header->next;
    }

    if (count != num_packets)
        return __LINE__;

    free(buffer);
    return 0;
}

int
main(int argc, char *argv[])
{
    runTest(null_pointer);
    runTest(not_aligned);
    runTest(no_errors);

    return 0;
}
