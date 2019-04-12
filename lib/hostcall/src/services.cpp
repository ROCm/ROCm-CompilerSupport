#include "services.h"
#include <amd_hostcall.h>
#include <string.h>

static int
function_call_handler(void *state, uint32_t service, uint64_t *payload)
{
    uint64_t output[2];

    auto fptr = reinterpret_cast<amd_hostcall_function_call_t>(payload[0]);
    fptr(output, payload + 1);
    memcpy(payload, output, 16);
    return 0;
}

void
register_services(void)
{
    amd_hostcall_register_service(SERVICE_FUNCTION_CALL, function_call_handler,
                                  nullptr);
}
