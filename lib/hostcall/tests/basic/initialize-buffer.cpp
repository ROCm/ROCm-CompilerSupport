#include "common.h"

#include <iostream>

void
test_null_pointer()
{
    ASSERT(amd_hostcall_initialize_buffer(nullptr, 0) ==
           AMD_HOSTCALL_ERROR_NULLPTR);
}

void
test_not_aligned()
{
    uint32_t ignored = -1;
    void *misaligned_buffer = (void *)1;

    ASSERT(amd_hostcall_initialize_buffer(misaligned_buffer, ignored) ==
           AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT);
}

void
test_no_errors()
{
    const uint32_t num_packets = 42;

    void *buffer = create_buffer(num_packets);
    ASSERT(buffer);
    void *aligned_buffer = realign_buffer(buffer);

    CHECK(amd_hostcall_initialize_buffer(aligned_buffer, num_packets));
    buffer_t *hb = (buffer_t *)aligned_buffer;

    ASSERT((uintptr_t)hb->payloads % alignof(payload_t) == 0);

    ASSERT((uintptr_t)hb->headers % alignof(header_t) == 0);

    ASSERT(hb->ready_stack == 0);

    ASSERT(hb->free_stack);

    uint64_t iter = hb->free_stack;
    uint32_t count = 0;
    while (iter) {
        ++count;
        auto header = get_header(hb, iter);
        iter = header->next;
    }

    ASSERT(count == num_packets);

    free(buffer);
}

int
main(int argc, char *argv[])
{
    ASSERT(set_flags(argc, argv) == 0);

    test_null_pointer();
    test_not_aligned();
    test_no_errors();

    test_passed();
}
