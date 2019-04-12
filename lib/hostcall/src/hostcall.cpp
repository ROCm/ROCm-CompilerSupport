#include "services.h"
#include <hostcall_impl.h>

#include <hsa/hsa.h>

#include <assert.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unordered_map>

#ifndef NDEBUG
int debug_mode = 0;
#endif // NDEBUG

#define GET_FUNCTION(ptr, name)                                                \
    auto ptr = (decltype(name) *)dlsym(RTLD_DEFAULT, #name);

// FIXME: Create one or more explicit vtables for all dlsym'ed
// functions symbols. These should be populated once from a
// hypothetical init() call.
GET_FUNCTION(my_hsa_signal_wait, hsa_signal_wait_acquire);

typedef enum {
    SIGNAL_INIT = UINT64_MAX,
    SIGNAL_DONE = UINT64_MAX - 1
} signal_value_t;

static uint32_t
set_control_field(uint32_t control, uint8_t offset, uint8_t width,
                  uint32_t value)
{
    uint32_t mask = ~(((1 << width) - 1) << offset);
    control &= mask;
    return control | (value << offset);
}

static uint32_t
reset_ready_flag(uint32_t control)
{
    return set_control_field(control, CONTROL_OFFSET_READY_FLAG,
                             CONTROL_WIDTH_READY_FLAG, 0);
}

static uint64_t
get_ptr_index(uint64_t ptr, uint32_t index_size)
{
    return ptr & ((1UL << index_size) - 1);
}

static uintptr_t
align_to(uintptr_t value, uint32_t alignment)
{
    if (value % alignment == 0) {
        return value;
    }
    return value - (value % alignment) + alignment;
}

static uintptr_t
get_header_start()
{
    return align_to(sizeof(buffer_t), alignof(header_t));
}

static uintptr_t
get_payload_start(uint32_t num_packets)
{
    auto header_start = get_header_start();
    auto header_end = header_start + sizeof(header_t) * num_packets;
    return align_to(header_end, alignof(payload_t));
}

static signal_t
create_signal()
{
    GET_FUNCTION(hsc, hsa_signal_create);
    if (!hsc) {
        WHEN_DEBUG(std::cerr << "no hsa" << std::endl);
        return {0};
    }
    hsa_signal_t hs;
    hsa_status_t status = hsc(SIGNAL_INIT, 0, NULL, &hs);
    if (status != HSA_STATUS_SUCCESS) {
        return { 0 };
    };
    return {hs.handle};
}

static uint64_t
wait_on_signal(signal_t doorbell, uint64_t timeout, uint64_t old_value)
{
    WHEN_DEBUG(std::cout << std::endl
                         << "old signal value: " << (int64_t)old_value
                         << std::endl);

    hsa_signal_t hs{doorbell.handle};
    assert(my_hsa_signal_wait);
    while (true) {
        uint64_t new_value =
            my_hsa_signal_wait(hs, HSA_SIGNAL_CONDITION_NE, old_value, timeout,
                               HSA_WAIT_STATE_BLOCKED);
        WHEN_DEBUG(std::cout << "\nnew signal value: " << new_value
                             << std::endl);
        if (new_value != old_value) {
            return new_value;
        }
    }
}

struct record_t {
    bool discarded;
};

typedef std::function<int(uint64_t *)> service_handler_t;
typedef std::function<void(int)> error_handler_t;

struct rwlock {
    pthread_rwlock_t _lock;

    rwlock()
    {
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_setkind_np(
            &attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
        pthread_rwlock_init(&_lock, &attr);
    }

    ~rwlock() { pthread_rwlock_destroy(&_lock); }

    void read_lock() { pthread_rwlock_rdlock(&_lock); }

    void write_lock() { pthread_rwlock_wrlock(&_lock); }

    void unlock() { pthread_rwlock_unlock(&_lock); }
};

/** \brief Global state maintained for all consumers.
 *
 *  The global state can only be initialized or modified before
 *  launching any consumers, since accesses from the consumer are not
 *  thread-safe.
 */
struct global_state_t {
    error_handler_t error_handler;
    std::unordered_map<uint32_t, service_handler_t> service_handlers;
    rwlock lock;

    global_state_t();
};

template <typename T> struct read_guard_t {
    read_guard_t(T &_d) : data(&_d) { data->lock.read_lock(); };

    ~read_guard_t() { data->lock.unlock(); }

  private:
    T *data;
};

template <typename T> struct write_guard_t {
    write_guard_t(T &_d) : data(&_d) { data->lock.write_lock(); };

    ~write_guard_t() { data->lock.unlock(); }

  private:
    T *data;
};

global_state_t::global_state_t() { register_services(); }

// FIXME: Replace the static constructor with explicit init() and
// shutdown() APIS.
static global_state_t g_state;

struct consumer_state_t {
    std::unordered_map<buffer_t *, record_t> buffers;
    rwlock lock;
};

/** \brief Encapsulates the entire consumer thread functionality.
 *
 *  The C API exposed in the header is a thin wrapper around this
 *  class. This ensures that the C++ interface is easy to expose if
 *  required.
 */
struct amd_hostcall_consumer_t {
  private:
    std::thread thread;
    consumer_state_t c_state;

    amd_hostcall_consumer_t(signal_t _doorbell) : doorbell(_doorbell) {}

  public:
    signal_t doorbell;
    amd_hostcall_error_t register_buffer(void *buffer);
    amd_hostcall_error_t deregister_buffer(void *buffer);

    void process_packets(buffer_t *buffer, uint64_t F) const;
    // FIXME: This cannot be const because it locks critical data. A
    // lock-free implementaiton might make that possible.
    void consume_packets();

    amd_hostcall_error_t launch();
    amd_hostcall_error_t terminate();

    static amd_hostcall_consumer_t *create();
};

static uint64_t
grab_ready_stack(buffer_t *buffer)
{
    return __atomic_exchange_n(&buffer->ready_stack, 0,
                               std::memory_order_acquire);
}

static header_t *
get_header(buffer_t *buffer, ulong ptr)
{
    return buffer->headers + get_ptr_index(ptr, buffer->index_size);
}

static payload_t *
get_payload(buffer_t *buffer, ulong ptr)
{
    return buffer->payloads + get_ptr_index(ptr, buffer->index_size);
}

void
amd_hostcall_consumer_t::process_packets(buffer_t *buffer,
                                         uint64_t ready_stack) const
{
    // This function is always called from consume_packets, which owns
    // the lock for the critical data.

    WHEN_DEBUG(std::cout << "process packets starting with " << ready_stack
                         << std::endl);

    // Each wave can submit at most one packet at a time, and all
    // waves independently push ready packets. The stack of packets at
    // this point cannot contain multiple packets from the same wave,
    // so consuming ready packets in a latest-first order does not
    // affect any wave.
    for (decltype(ready_stack) iter = ready_stack, next = 0; iter;
         iter = next) {
        WHEN_DEBUG(std::cout << "processing ptr: " << iter << std::endl);
        WHEN_DEBUG(std::cout << "packet index: " << std::dec
                             << get_ptr_index(iter, buffer->index_size)
                             << std::endl);

        // Remember the next packet pointer. The current packet will
        // get reused from the free stack after we process it.
        auto header = get_header(buffer, iter);
        next = header->next;

        auto service = header->service;
        WHEN_DEBUG(std::cout << "packet service: " << (uint32_t)service
                             << std::endl);

        auto h_iter = g_state.service_handlers.find(service);
        if (h_iter == g_state.service_handlers.end()) {
            h_iter = g_state.service_handlers.find(SERVICE_DEFAULT);
        }
        if (h_iter == g_state.service_handlers.end()) {
            std::cerr << std::endl
                      << "hostcall fatal error: no handler found for service "
                      << (uint32_t)service << std::endl;
            if (g_state.error_handler) {
                g_state.error_handler(AMD_HOSTCALL_ERROR_SERVICE_UNKNOWN);
            }
            std::quick_exit(EXIT_FAILURE);
        }
        auto handler = h_iter->second;
        auto payload = get_payload(buffer, iter);
        uint64_t activemask = header->activemask;
        WHEN_DEBUG(std::cout << "activemask: " << std::hex << activemask
                             << std::endl);

        // TODO: One could use ffs to skip inactive lanes faster.
        for (uint32_t wi = 0; wi != 64; ++wi) {
            uint64_t flag = activemask & ((uint64_t)1 << wi);
            if (flag == 0) {
                continue;
            }
            uint64_t *slot = payload->slots[wi];
            auto retval = handler(slot);
            if (retval != 0) {
                std::cerr << std::endl
                          << "hostcall fatal error: handler for service "
                          << (uint32_t)service << " returned failure\n"
                          << amd_hostcall_error_string(retval) << "(" << retval
                          << ")" << std::endl;
                if (g_state.error_handler) {
                    g_state.error_handler(retval);
                }
                std::quick_exit(EXIT_FAILURE);
            }
        }

        __atomic_store_n(&header->control, reset_ready_flag(header->control),
                         std::memory_order_release);
    }
}

void
amd_hostcall_consumer_t::consume_packets()
{
    /* TODO: The consumer iterates over all registered buffers in an
       unspecified order, and for each buffer, processes packets also
       in an unspecified order. This may need a more efficient
       strategy based on the turnaround time for the services
       requested by all these packets.
     */
    WHEN_DEBUG(std::cout << "launched consumer" << std::endl);
    uint64_t signal_value = SIGNAL_INIT;
    uint64_t timeout = 1024 * 1024;

    while (true) {
        signal_value = wait_on_signal(doorbell, timeout, signal_value);
        if (signal_value == SIGNAL_DONE) {
            return;
        }

        read_guard_t<consumer_state_t> c_lock(c_state);
        read_guard_t<global_state_t> g_lock(g_state);

        for (auto ii = c_state.buffers.begin(), ie = c_state.buffers.end();
             ii != ie;
             /* don't increment here */) {
            auto record = ii->second;
            if (record.discarded) {
                ii = c_state.buffers.erase(ii);
                continue;
            }

            buffer_t *buffer = ii->first;
            uint64_t F = grab_ready_stack(buffer);
            WHEN_DEBUG(std::cout << "grabbed ready stack: " << F << std::endl);
            if (F) {
                process_packets(buffer, F);
            }
            ++ii;
        }
    }

    return;
}

amd_hostcall_error_t
amd_hostcall_consumer_t::launch()
{
    if (thread.joinable()) {
        return AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE;
    }

    thread = std::thread(&amd_hostcall_consumer_t::consume_packets, this);
    if (!thread.joinable()) {
        return AMD_HOSTCALL_ERROR_CONSUMER_LAUNCH_FAILED;
    }

    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_consumer_t::terminate()
{
    if (!thread.joinable()) {
        return AMD_HOSTCALL_SUCCESS;
    }
    hsa_signal_t signal = {doorbell.handle};
    GET_FUNCTION(hssr, hsa_signal_store_release);
    assert(hssr);
    hssr(signal, SIGNAL_DONE);
    thread.join();
    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_consumer_t::register_buffer(void *b)
{
    write_guard_t<consumer_state_t> c_lock(c_state);
    auto buffer = reinterpret_cast<buffer_t *>(b);

    auto &record = c_state.buffers[buffer];
    WHEN_DEBUG(std::cout << "registered buffer: " << std::hex << b
                         << std::endl);
    record.discarded = false;
    buffer->doorbell = doorbell;
    WHEN_DEBUG(std::cout << "signal: " << buffer->doorbell.handle << std::endl);
    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_consumer_t::deregister_buffer(void *b)
{
    write_guard_t<consumer_state_t> c_lock(c_state);
    auto buffer = reinterpret_cast<buffer_t *>(b);
    if (c_state.buffers.count(buffer) == 0) {
        return AMD_HOSTCALL_ERROR_INVALID_REQUEST;
    }
    auto &record = c_state.buffers[buffer];
    if (record.discarded) {
        return AMD_HOSTCALL_ERROR_INVALID_REQUEST;
    }
    record.discarded = true;
    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_consumer_t *
amd_hostcall_consumer_t::create()
{
    signal_t doorbell = create_signal();
    if (doorbell.handle == 0) {
        return nullptr;
    }
    return new amd_hostcall_consumer_t(doorbell);
}

amd_hostcall_error_t
amd_hostcall_create_consumer(amd_hostcall_consumer_t **consumer)
{
    *consumer = amd_hostcall_consumer_t::create();
    if (*consumer) {
        return AMD_HOSTCALL_SUCCESS;
    }
    return AMD_HOSTCALL_INTERNAL_ERROR;
}

size_t
amd_hostcall_get_buffer_size(uint32_t num_packets)
{
    WHEN_DEBUG(std::cout << "header start: " << get_header_start()
                         << std::endl);
    WHEN_DEBUG(std::cout << "payload start: " << get_payload_start(num_packets)
                         << std::endl);
    size_t buffer_size = get_payload_start(num_packets);
    buffer_size += num_packets * sizeof(payload_t);
    return buffer_size;
}

uint32_t
amd_hostcall_get_buffer_alignment()
{
    return alignof(payload_t);
}

amd_hostcall_error_t
amd_hostcall_initialize_buffer(void *buffer, uint32_t num_packets)
{
    if (!buffer) {
        return AMD_HOSTCALL_ERROR_NULLPTR;
    }

    if ((uintptr_t)buffer % amd_hostcall_get_buffer_alignment() != 0) {
        return AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT;
    }

    buffer_t *hb = (buffer_t *)buffer;

    hb->headers = (header_t *)((uint8_t *)hb + get_header_start());
    hb->payloads =
        (payload_t *)((uint8_t *)hb + get_payload_start(num_packets));

    uint32_t index_size = 1;
    if (num_packets > 2) {
        index_size = 32 - __builtin_clz(num_packets);
    }
    WHEN_DEBUG(std::cout << "index size: " << index_size << std::endl);
    hb->index_size = index_size;
    hb->headers[0].next = 0;

    uint64_t next = 1UL << index_size;
    for (uint32_t ii = 1; ii != num_packets; ++ii) {
        hb->headers[ii].next = next;
        next = ii;
    }
    hb->free_stack = next;
    hb->ready_stack = 0;

    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_register_service(uint32_t service,
                              amd_hostcall_service_handler_t handler,
                              void *state)
{
    write_guard_t<global_state_t> g_lock(g_state);
    if (g_state.service_handlers.count(service) != 0) {
        return AMD_HOSTCALL_ERROR_INVALID_REQUEST;
    }
    g_state.service_handlers[service] =
        std::bind(handler, state, service, std::placeholders::_1);
    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_register_buffer(amd_hostcall_consumer_t *consumer, void *buffer)
{
    return consumer->register_buffer(buffer);
}

amd_hostcall_error_t
amd_hostcall_deregister_buffer(amd_hostcall_consumer_t *consumer, void *buffer)
{
    return consumer->deregister_buffer(buffer);
}

amd_hostcall_error_t
amd_hostcall_launch_consumer(amd_hostcall_consumer_t *consumer)
{
    return consumer->launch();
}

amd_hostcall_error_t
amd_hostcall_destroy_consumer(amd_hostcall_consumer_t *consumer)
{
    auto err = consumer->terminate();
    if (err != AMD_HOSTCALL_SUCCESS) {
        return err;
    }

    GET_FUNCTION(hsd, hsa_signal_destroy);
    assert(hsd);
    hsa_signal_t hs{consumer->doorbell.handle};
    if (hsd(hs) != HSA_STATUS_SUCCESS) {
        return AMD_HOSTCALL_INTERNAL_ERROR;
    }
    delete consumer;
    return AMD_HOSTCALL_SUCCESS;
}

amd_hostcall_error_t
amd_hostcall_on_error(amd_hostcall_error_handler_t handler, void *state)
{
    write_guard_t<global_state_t> g_lock(g_state);
    if (g_state.error_handler) {
        return AMD_HOSTCALL_ERROR_INVALID_REQUEST;
    }

    using namespace std::placeholders;
    g_state.error_handler = std::bind(handler, state, _1);
    return AMD_HOSTCALL_SUCCESS;
}

void
amd_hostcall_enable_debug()
{
#ifndef NDEBUG
    debug_mode = true;
#endif
}

const char *
amd_hostcall_error_string(int error)
{
    switch (error) {
    case AMD_HOSTCALL_SUCCESS:
        return "AMD_HOSTCALL_SUCCESS";
    case AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE:
        return "AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE";
    case AMD_HOSTCALL_ERROR_CONSUMER_INACTIVE:
        return "AMD_HOSTCALL_ERROR_CONSUMER_INACTIVE";
    case AMD_HOSTCALL_ERROR_CONSUMER_LAUNCH_FAILED:
        return "AMD_HOSTCALL_ERROR_CONSUMER_LAUNCH_FAILED";
    case AMD_HOSTCALL_ERROR_INVALID_REQUEST:
        return "AMD_HOSTCALL_ERROR_INVALID_REQUEST";
    case AMD_HOSTCALL_ERROR_SERVICE_UNKNOWN:
        return "AMD_HOSTCALL_ERROR_SERVICE_UNKNOWN";
    case AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT:
        return "AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT";
    case AMD_HOSTCALL_ERROR_NULLPTR:
        return "AMD_HOSTCALL_ERROR_NULLPTR";
    case AMD_HOSTCALL_INTERNAL_ERROR:
        return "AMD_HOSTCALL_INTERNAL_ERROR";
    default:
        return "AMD_HOSTCALL_ERROR_UNKNOWN";
    }
}
