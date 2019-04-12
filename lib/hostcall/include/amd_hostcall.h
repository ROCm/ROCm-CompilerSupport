#ifndef AMD_HOSTCALL_H
#define AMD_HOSTCALL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef AMD_HOSTCALL_EXPORT_DECORATOR
#ifdef __GNUC__
#define AMD_HOSTCALL_EXPORT_DECORATOR __attribute__((visibility("default")))
#else
#define AMD_HOSTCALL_EXPORT_DECORATOR __declspec(dllexport)
#endif
#endif

#ifndef AMD_HOSTCALL_IMPORT_DECORATOR
#ifdef __GNUC__
#define AMD_HOSTCALL_IMPORT_DECORATOR
#else
#define AMD_HOSTCALL_IMPORT_DECORATOR __declspec(dllimport)
#endif
#endif

#ifndef AMD_HOSTCALL_API
#ifdef AMD_HOSTCALL_EXPORT
#define AMD_HOSTCALL_API AMD_HOSTCALL_EXPORT_DECORATOR
#else
#define AMD_HOSTCALL_API AMD_HOSTCALL_IMPORT_DECORATOR
#endif
#endif

/** \file Support library for invoking host services from the device.
 *
 *  The hostcall consumer defined here is used by the language runtime
 *  to serve requests originating from kernels running on GPU
 *  devices. A typical flow is as follows:
 *
 *  - Create and launch one or more hostcall consumers.
 *  - Create and initialize a hostcall buffer per command queue.
 *  - Register these buffers with the appropriate consumer.
 *  - When a buffer is no longer used, deregister and then free it.
 *  - Destroy the consumer(s) when they are no longer required. Must be
 *    done before exiting the application, so that the consumer
 *    threads can join() correctly.
 *
 *  For a more information, see the accompanying README and the
 *  comments associated with each of the API functions.
 */

typedef enum {
    AMD_HOSTCALL_SUCCESS,
    AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE,
    AMD_HOSTCALL_ERROR_CONSUMER_INACTIVE,
    AMD_HOSTCALL_ERROR_CONSUMER_LAUNCH_FAILED,
    AMD_HOSTCALL_ERROR_INVALID_REQUEST,
    AMD_HOSTCALL_ERROR_SERVICE_UNKNOWN,
    AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT,
    AMD_HOSTCALL_ERROR_NULLPTR,
    AMD_HOSTCALL_INTERNAL_ERROR
} amd_hostcall_error_t;

AMD_HOSTCALL_API
const char *
amd_hostcall_error_string(int error);

/// Opaque struct that encapsulates a consumer thread.
typedef struct amd_hostcall_consumer_t amd_hostcall_consumer_t;

/** \brief Create a consumer instance that tracks a single consumer thread.
 *
 *  Each instance manages a unique consumer thread, along with a list
 *  of hostcall buffers that this thread processes. The consumer does
 *  not occupy any resources other than it's own memory footprint
 *  until it is launched.
 *
 *  The corresponding consumer thread must be launched for the
 *  consumer to perform any actual work. The consumer thread can be
 *  launched even without any buffers registered with the
 *  consumer. The API provides thread-safe methods to register buffers
 *  with an active consumer.
 *
 *  A single consumer is sufficient to correctly handle all hostcall
 *  buffers created in the application. The client may safely launch
 *  multiple consumers based on factors external to this library.
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_INTERNAL_ERROR if an internal error occurred.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_create_consumer(amd_hostcall_consumer_t **consumer);

/** \brief Destroy a consumer instance.
 *
 *  If the consumer is active, the corresponding thread is terminated
 *  and join()'ed to the current thread.
 *
 *  Behavious is undefined when called multiple times on the same
 *  pointer, or using a pointer that was not previously created by
 *  amd_hostcall_create_consumer().
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_INTERNAL_ERROR if an internal error occurred.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_destroy_consumer(amd_hostcall_consumer_t *consumer);

/** \brief Function invoked on each workitem payload.
 *  \param state   External state supplied by the client.
 *  \param service Service ID received on the packet. This allows a
 *                 single function to be registered for multiple service IDs
 *  \param payload Pointer to an array of eight 64-bit integer values.
 *  \return Success/failure encoded as an integer.
 *
 *  For each packet received from the device, the consumer locates and
 *  invokes the corresponding service handler. The handler runs in the
 *  same thread as the consumer. The service-handler must ensure
 *  thread-safe access to #state when used with multiple consumers.
 *
 *  The parameter #payload serves as both input and output to the
 *  service handler. It originally contains the data transmitted by
 *  the device; the handler processes this data and writes its
 *  response to the same location.
 *
 *  The payload was successfully handled if the handler returns a
 *  zero. If the handler returns a non-zero value, the consumer will
 *  invoke the error handler (if registered) and cause the application
 *  to exit. Since pre-defined errors (amd_hostcall_error_t) are
 *  positive values, service handlers should use negative values to
 *  avoid a collision.
 */
typedef int (*amd_hostcall_service_handler_t)(void *state, uint32_t service,
                                              uint64_t *payload);

/** \brief Register a service handler.
 *  \param service Numerical ID for the service being registered.
 *  \param handler Function to be invoked for a given #service ID.
 *  \param state   External state supplied by the client.
 *  \return Error code indicating success or specific failure.
 *
 *  *** INTERNAL USE ONLY ***
 *  The list of supported services is defined internally by the
 *  implementation. Behaviour is undefined if an application attempts
 *  to register a service.
 *
 *  The service ID "0" (zero) is reserved as the default handler. The
 *  default handler is invoked when a received packet specifies a
 *  service with no registered handler.
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_ERROR_INVALID_REQUEST if the service is
 *         already registered.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_register_service(uint32_t service,
                              amd_hostcall_service_handler_t handler,
                              void *state);

/** \brief Determine the buffer size to be allocated for the given
 *         number of packets.
 *
 *  The reported size includes any internal padding required for the
 *  packets and their headers.
 */
AMD_HOSTCALL_API
size_t
amd_hostcall_get_buffer_size(uint32_t num_packets);

/** \brief Alignment required for the start of the buffer.
 */
AMD_HOSTCALL_API
uint32_t
amd_hostcall_get_buffer_alignment(void);

/** \brief Initialize the buffer data-structure.
 *  \param buffer      Pointer to allocated buffer.
 *  \param num_packets Number of packets to be created in the buffer.
 *  \return Error code indicating success or specific failure.
 *
 *  The function assumes that the supplied buffer is sufficiently
 *  large to accomodate the specified number of packets. The value
 *  returned is one of:
 *
 *  \li \c AMD_HOSTCALL_SUCCESS on successful initialization.
 *  \li \c AMD_HOSTCALL_ERROR_NULLPTR if the supplied pointer is NULL.
 *  \li \c AMD_HOSTCALL_ERROR_INCORRECT_ALIGNMENT if the supplied
 *      pointer is not aligned to the value returned by
 *      amd_hostcall_get_buffer_alignment().
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_initialize_buffer(void *buffer, uint32_t num_packets);

/** \brief Register a buffer with a consumer.
 *
 *  Behaviour is undefined if:
 *  - amd_hostcall_initialize_buffer() was not invoked successfully on
 *    the buffer prior to registration.
 *  - The same buffer is registered with multiple consumers.
 *
 *  The function has no effect if the a buffer is registered multiple
 *  times with the same consumer.
 *
 *  The client must register a buffer before launching any kernel that
 *  accesses that buffer. The client must further ensure that each
 *  buffer is associated with a unique command queue across all
 *  devices.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_register_buffer(amd_hostcall_consumer_t *consumer, void *buffer);

/** \brief Deregister a buffer that is no longer in use.
 *
 *  The client may free this buffer after deregistering it from the
 *  corresponding consumer. Behaviour is undefined if the buffer is
 *  freed without first deregistering it from the consumer.
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_ERROR_INVALID_REQUEST if the buffer was
 *      previously deregistered or not registered with this consumer.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_deregister_buffer(amd_hostcall_consumer_t *consumer, void *buffer);

/** \brief Launch the consumer in its own thread.
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_ERROR_CONSUMER_ACTIVE if the thread is already
 *      running. Such a call has no effect on the consumer thread.
 *  \li \c AMD_HOSTCALL_ERROR_CONSUMER_LAUNCH_FAILED if the thread was
 *      not already running and it failed to launch.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_launch_consumer(amd_hostcall_consumer_t *consumer);

/** \brief Function invoked when a non-recovereble error occurs.
 *  \param error Identifier for the non-recoverable error. The only
 *               valid value is AMD_HOSTCALL_ERROR_SERVICE_UNKNOWN,
 *               but the handler must gracefully handle any other
 *               values that may be defined in the future.
 *  \param state  External state supplied by the client.
 *
 *  This method is invoked by a consumer when it encounters a
 *  non-recoverable error. It runs in the same thread as the
 *  consumer. The handler must ensure thread-safe access to #state
 *  when used with multiple consumers.
 */
typedef void (*amd_hostcall_error_handler_t)(void *state, int error);

/** \brief Register a handler for non-recoverable errors.
 *
 *  A non-recoverable error occurs when the consumer find a service
 *  handler for a received packet, and a default service handler is
 *  not registered. When that happens, the consumer invokes the error
 *  handler, and then causes the application to exit.
 *
 *  The value returned is one of:
 *  \li \c AMD_HOSTCALL_SUCCESS on success.
 *  \li \c AMD_HOSTCALL_ERROR_INVALID_REQUEST if an error handler is
 *         already registered.
 */
AMD_HOSTCALL_API
amd_hostcall_error_t
amd_hostcall_on_error(amd_hostcall_error_handler_t handler, void *state);

/** \brief Print debug messages to standard output.
 *
 *  Enabling debug in a release build has no effect.
 *
 *  TODO: Implement logging that can be controlled by the client.
 */
AMD_HOSTCALL_API
void
amd_hostcall_enable_debug(void);

/** \brief Signature for pointer accepted by the function call service.
 *  \param output Pointer to output arguments.
 *  \param input Pointer to input arguments.
 *
 *  The function can accept up to seven 64-bit arguments via the
 *  #input pointer, and can produce up to two 64-bit arguments via the
 *  #output pointer. The contents of these arguments are defined by
 *  the function being invoked.
 */
typedef void (*amd_hostcall_function_call_t)(uint64_t *output,
                                             const uint64_t *input);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AMD_HOSTCALL_H
