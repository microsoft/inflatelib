/*
 *
 */
#ifndef INFLATELIB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define INFLATELIB_VERSION_STRING "0.0.1"
#define INFLATELIB_VERSION_MAJOR 0
#define INFLATELIB_VERSION_MINOR 0
#define INFLATELIB_VERSION_PATCH 1

    typedef void* (*inflatelib_alloc)(void* userData, size_t bytes);
    typedef void (*inflatelib_free)(void* userData, void* allocatedPtr);

    struct inflatelib_state; /* Opaque to client applications */

    typedef struct inflatelib_stream
    {
        /*
         * Pointer to the next byte of input. This pointer is set by the caller and updated by the library based on the
         * number of bytes consumed.
         */
        const void* next_in;
        /*
         * The number of valid bytes available for read in 'next_in'. This value is set by the caller and updated by the
         * library based on the number of bytes consumed.
         */
        size_t avail_in;
        /*
         * Total number of bytes read by the library so far. This value is never consumed by the library; it is only
         * ever incremented.
         */
        uintmax_t total_in;

        /*
         * Pointer to the next byte of output data written by the library. This pointer is set by the caller and updated
         * by the library based on the number of bytes written so that this points one byte past the last byte of output
         * written.
         */
        void* next_out;
        /*
         * The number of bytes, starting at 'next_out', that can be written to. This value is set by the caller and
         * updated by the library based on the number of bytes written.
         */
        size_t avail_out;
        /*
         * Total number of bytes written by the library so far. This value is never consumed by the library; it is only
         * ever incremented.
         */
        uintmax_t total_out;

        /*
         * Optional user data passed to any callback functions (such as alloc/free below)
         */
        void* user_data;

        /*
         * Custom memory allocation functions. Set to null to get malloc/free respectively
         */
        inflatelib_alloc alloc;
        inflatelib_free free;

        /*
         * A string describing the last error encountered. This pointer is only valid if a library function returned
         * failure
         */
        const char* error_msg;

        /*
         * Internal state used by the library
         */
        struct inflatelib_state* internal;
    } inflatelib_stream;

/*
 * Return values. Non-negative values indicate success while negative values indicate some sort of error. When a
 * negative value is returned, the 'error_msg' member of the 'inflatelib_stream' will be set. Otherwise, the 'error_msg'
 * will remain unchanged. A positive return value indicates an "interesting" change in state that is not considered a
 * failure, while a return value of zero indicates generic success.
 */
#define INFLATELIB_OK 0          /* No error occurred */
#define INFLATELIB_EOF 1         /* No error occurred; reached the end of the stream */
#define INFLATELIB_ERROR_ARG -1  /* Invalid argument */
#define INFLATELIB_ERROR_DATA -2 /* Error in the input data */
#define INFLATELIB_ERROR_OOM -3  /* Failed to allocate data */

    /*
     * Initializes the stream. The 'user_data', 'alloc', and 'free' members MUST be set prior to the init call and MUST
     * NOT be changed after the init call completes. This function returns one of the status values specified above.
     */
    int inflatelib_init(inflatelib_stream* stream);

    /*
     * Resets the stream's state back to its initialized state. This allows the stream to be reused for multiple inflate
     * calls without having to destroy and reinitialize it. Typically, this is used to reset the stream after an inflate
     * call returns EOF, however it can also be used to reset the stream after an error or even during the middle of
     * inflating a stream. This function also allows the caller to switch between Deflate and Deflate64.
     */
    int inflatelib_reset(inflatelib_stream* stream);

    /*
     * Cleans up any data allocated/initialized by the library. This function must be called if 'inflatelib_init'
     * returns success. After this function is called, the 'inflatelib_stream' cannot be used for any function call
     * unless 'inflatelib_init' is called again. This function can only return success.
     */
    int inflatelib_destroy(inflatelib_stream* stream);

    /*
     *
     */
    int inflatelib_inflate(inflatelib_stream* stream);

    /*
     *
     */
    int inflatelib_inflate64(inflatelib_stream* stream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INFLATELIB_H
