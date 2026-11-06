/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

/** @file inflatelib.h
 * @brief InflateLib C API.
 *
 * This API can be used to decompress DEFLATE or DEFFLATE64 encoded data.
 *
 * Example decoding DEFLATE64 encoded data:
 * \code
 * #include <stdio.h>
 * #include <stdlib.h>
 * #include <string.h>
 *
 * #include <inflatelib.h>
 *
 * int main () {
 *   // As an example, the input contains the DEFLATE/DEFLATE64 encoded string "Hello world\n".
 *   size_t inputSize = 14;
 *   const char *input = "\xf3\x48\xcd\xc9\xc9\x57\x28\xcf\x2f\xca\x49\xe1\x02\x00";
 *
 *   // Initialize outputBuffer
 *   size_t outputBufferSize = 4096;
 *   char outputBuffer[outputBufferSize];
 *
 *   // Initialize stream.
 *   inflatelib_stream stream = { 0 };
 *   if (inflatelib_init(&stream) != INFLATELIB_OK) {
 *     printf("ERROR: %s\n", stream.error_msg);
 *     return 1;
 *   }
 *
 *   // Setup stream to read from input. Assume the whole input
 *   // stream is in memory. Otherwise we need to update these values
 *   // and read new data as necessary inside a loop.
 *   stream.next_in = input;
 *   stream.avail_in = inputSize;
 *
 *   // Decode the data from the input.
 *   int result = INFLATELIB_OK;
 *   while (result != INFLATELIB_EOF) {
 *     stream.next_out = outputBuffer;
 *     stream.avail_out = outputBufferSize;
 *
 *     result = inflatelib_inflate64(&stream);
 *     if (result < INFLATELIB_OK) {
 *       printf("ERROR: %s\n", stream.error_msg);
 *       inflatelib_destroy(&stream);
 *       return 1;
 *     }
 *
 *     fwrite(outputBuffer, 1, outputBufferSize - stream.avail_out, stdout);
 *   }
 *
 *   // Discard stream.
 *   inflatelib_destroy(&stream);
 *
 *   return 0;
 * };
 * \endcode
 */

#ifndef INFLATELIB_H
#define INFLATELIB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __has_attribute
#if __has_attribute(visibility)
#define INFLATELIB_HAS_VISIBILITY_ATTR 1
#endif
#endif

/** @cond
 *
 * INFLATELIB_BUILD_SHARED      The library is being built as a shared library
 * INFLATELIB_CONSUME_SHARED    The library is being consumed as a shared library
 * Otherwise...                 The library is either being built as a static library or it is being consumed in an
 *                              unspecified manner; either as a shared or static library.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#ifdef INFLATELIB_BUILD_SHARED
#define INFLATELIB_EXPORT __declspec(dllexport)
#elif defined(INFLATELIB_CONSUME_SHARED)
#define INFLATELIB_EXPORT __declspec(dllimport)
#else
#define INFLATELIB_EXPORT /* Unknown or static - don't decorate function declarations */
#endif
#elif INFLATELIB_HAS_VISIBILITY_ATTR
#define INFLATELIB_EXPORT __attribute__((visibility("default")))
#else
#define INFLATELIB_EXPORT /* Not Windows and no visibility attribute... don't decorate function declarations */
#endif
/** @endcond */

/** @cond */
#if defined(_WIN32) && defined(_MSC_VER)
#define INFLATELIB_CALLCONV __cdecl
#else
#define INFLATELIB_CALLCONV
#endif
/** @endcond */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief The library version of the headers.
 */
#define INFLATELIB_VERSION_STRING "0.3.0"

/**
 * \brief The major version number of the library headers.
 */
#define INFLATELIB_VERSION_MAJOR 0

/**
 * \brief The minor version number of the library headers.
 */
#define INFLATELIB_VERSION_MINOR 3

/**
 * \brief The patch version number of the library headers.
 */
#define INFLATELIB_VERSION_PATCH 0

    /**
     * @brief Function pointer type for a custom memory allocation callback.
     *
     * The allocation callback is used by InflateLib to request aligned memory blocks.
     *
     * @param[in,out] userData User-specified context pointer passed from the library host.
     * @param[in] bytes The number of bytes to allocate.
     * @param[in] alignment The required memory alignment boundary (in bytes).
     *
     * @return A pointer to the allocated memory block, or @c NULL if allocation fails.
     */
    typedef void* (*inflatelib_alloc)(void* userData, size_t bytes, size_t alignment);

    /**
     * @brief Function pointer type for a custom memory deallocation callback.
     *
     * The deallocation callback is used by InflateLib to free memory blocks previously allocated by the corresponding allocation
     * callback.
     *
     * @param[in,out] userData User-specified context pointer passed from the library host.
     * @param[in] allocatedPtr Pointer to the memory block to be freed. If @c NULL, no action is taken.
     * @param[in] bytes The size of the allocated block (must match the original allocation size).
     * @param[in] alignment The alignment boundary of the allocated block (must match the original allocation alignment).
     */
    typedef void (*inflatelib_free)(void* userData, void* allocatedPtr, size_t bytes, size_t alignment);

    /**
     * Opaque to client applications
     */
    struct inflatelib_state;

    /**
     * @brief This struct stores all data and state for decompressing DEFLATE/DEFLATE64 encoded data.
     *
     * With the exception of success/error codes, all input and output data is passed through this struct. Specifically, input and
     * output buffers are set by the caller via `next_in`/`avail_in` and `next_out`/`avail_out` respectively. The
     * inflatelib_inflate() and inflatelib_inflate64() functions communicate the amount of data consumed/written by updating these
     * buffers and lengths.
     *
     * For example, you can determine the amount of data consumed/written by a call to inflatelib_inflate() or
     * inflatelib_inflate64() by either subtracting `next_in`/`next_out` after the call by their original pointers (interpreted as
     * @c uintptr_t values) or by subtracting `avail_in`/`avail_out` after the call from the sizes passed in. Alternatively, you
     * can subtract `total_in`/`total_out` after the call by their values before the call (or equivalently by setting them to zero
     * before the call and reading their values after the call).
     *
     * Internally, these objects behave as a state machine. After being initialized, they transition to either "Deflate" or
     * "Deflate64" decoding states via calls to inflatelib_inflate() and inflatelib_inflate64() respectively. When in these
     * states, the only valid operations are to destroy the object, put the object back into the initialized state via
     * inflatelib_reset(), or to decompress more data using the function that corresponds to the current state: either
     * inflatelib_inflate() for the "Deflate" state or inflatelib_inflate64() for the "Deflate64" state. If you attempt to call
     * inflatelib_inflate64() when in the "Deflate" state or vice-versa, the call will fail.
     */
    typedef struct inflatelib_stream
    {
        /**
         * @brief Pointer to the next byte of input.
         *
         * This pointer is set by the caller and updated by the library based on the number of bytes consumed.
         */
        const void* next_in;

        /**
         * @brief The number of valid bytes available for read in 'next_in'.
         *
         * This value is set by the caller and updated by the library based on the number of bytes consumed.
         */
        size_t avail_in;

        /**
         * @brief Total number of bytes read by the library so far.
         *
         * This value is never consumed by the library; it is only ever incremented.
         */
        uintmax_t total_in;

        /**
         * @brief Pointer to the next byte of output data written by the library.
         *
         * This pointer is set by the caller and updated by the library based on the number of bytes written so that this points
         * one byte past the last byte of output written.
         */
        void* next_out;

        /**
         * @brief The number of bytes, starting at 'next_out', that can be written to.
         *
         * This value is set by the caller and updated by the library based on the number of bytes written.
         */
        size_t avail_out;

        /**
         * @brief Total number of bytes written by the library so far.
         *
         * This value is never consumed by the library; it is only ever incremented.
         */
        uintmax_t total_out;

        /**
         * @brief Optional user data.
         *
         * Passed to any callback functions (such as alloc/free below).
         */
        void* user_data;

        /**
         * @brief Custom memory allocation function.
         *
         * Set to @c null to get @c malloc.
         */
        inflatelib_alloc alloc;

        /**
         * @brief Custom memory deallocation function.
         *
         * Set to @c null to get @c free.
         */
        inflatelib_free free;

        /**
         * @brief A string describing the last error encountered.
         *
         * This pointer is only valid if a library function returned failure.
         */
        const char* error_msg;

        /**
         * @brief Internal state used by the library.
         */
        struct inflatelib_state* internal;
    } inflatelib_stream;

    /**
     * @defgroup RETURN_VALUES_GROUP Return values
     * @brief Return values for inflatelib_init(), inflatelib_inflate(), inflatelib_inflate64(), inflatelib_reset() and
     * inflatelib_destroy().
     *
     * Non-negative values indicate success while negative values indicate some sort of error. When a negative value is returned,
     * the `error_msg` member of the #inflatelib_stream will be set. Otherwise, the `error_msg` will remain unchanged. A positive
     * return value indicates an "interesting" change in state that is not considered a failure, while a return value of zero
     * indicates generic success.
     *
     * @{
     */

/**
 * @brief No error occured.
 */
#define INFLATELIB_OK 0

/**
 * @brief No error occurred; reached the end of the stream.
 */
#define INFLATELIB_EOF 1

/**
 * @brief Invalid argument error.
 */
#define INFLATELIB_ERROR_ARG -1

/**
 * @brief Error in the input data.
 */
#define INFLATELIB_ERROR_DATA -2

/**
 * @brief Failed to allocate memory.
 */
#define INFLATELIB_ERROR_OOM -3

/**
 * @brief The linked library has an incompatible major version.
 */
#define INFLATELIB_ERROR_VERSION -4

    /* End of RETURN_VALUES_GROUP */
    /**
     * @}
     */

    /**
     * @brief Returns the version of the linked InflateLib library.
     *
     * The implementation just returns #INFLATELIB_VERSION_STRING, however this may differ when linking, statically or
     * dynamically, against a pre-built library.
     *
     * The major version of #INFLATELIB_VERSION_STRING must match the major version returned by this function, otherwise the two
     * are incompatible (struct size, arrangement, etc. may differ).
     *
     * The inflatelib_init() macro does this for you by passing #INFLATELIB_VERSION_STRING to inflatelib_init_().
     *
     * @return The version of the linked InflateLib library as a string in the form 'X.Y.Z'.
     */
    INFLATELIB_EXPORT const char* INFLATELIB_CALLCONV inflatelib_version(void);

/**
 * @brief Verifies library version and initializes the stream.
 *
 * Verifies that the major version of the linked InflateLib library matches the major version of these library headers and
 * initializes the \p stream.
 *
 * The `user_data`, `alloc` and `free` members of \p stream MUST be set prior to the inflatelib_init() call and MUST NOT be
 * changed after the inflatelib_init() call completes.
 *
 * @param[in,out] stream A pointer to an #inflatelib_stream.
 *
 * @retval #INFLATELIB_OK On success.
 * @retval #INFLATELIB_ERROR_VERSION When the linked library has an incompatible major version.
 * @retval #INFLATELIB_ERROR_OOM When it fails allocating memory.
 */
#define inflatelib_init(stream) inflatelib_init_(stream, INFLATELIB_VERSION_STRING)

    /**
     * @brief Verifies required library version and initializes the stream.
     *
     * Verifies that the major version of the linked InflateLib library matches the caller's major version and initializes the \p
     * stream.
     *
     * The `user_data`, `alloc`, and `free` members of \p stream MUST be set prior to the inflatelib_init_() call and MUST NOT be
     * changed after the inflatelib_init_() call completes.
     *
     * @param[in,out] stream A pointer to an #inflatelib_stream.
     * @param[in] version The required library version.
     *
     * @retval #INFLATELIB_OK On success.
     * @retval #INFLATELIB_ERROR_VERSION When the linked library is incompatible with the requested version.
     * @retval #INFLATELIB_ERROR_OOM When it fails allocating memory.
     *
     */
    INFLATELIB_EXPORT int INFLATELIB_CALLCONV inflatelib_init_(inflatelib_stream* stream, const char* version);

    /**
     * @brief Resets the stream's state back to its initialized state.
     *
     * This allows the stream to be reused for multiple inflate calls without having to destroy and reinitialize it. Typically,
     * this is used to reset the stream after an inflatelib_inflate() or an inflatelib_inflate64() call returns #INFLATELIB_EOF,
     * however it can also be used to reset the stream after an error or even during the middle of inflating a stream.
     *
     * This function also allows the caller to switch between Deflate and Deflate64.
     *
     * @param[in,out] stream A pointer to an #inflatelib_stream previously initialized by inflatelib_init().
     *
     * @retval #INFLATELIB_OK On success.
     * @retval #INFLATELIB_ERROR_ARG When \p stream is not initialized correctly.
     */
    INFLATELIB_EXPORT int INFLATELIB_CALLCONV inflatelib_reset(inflatelib_stream* stream);

    /**
     * @brief Cleans up any data allocated/initialized by the library.
     *
     * This function must be called if inflatelib_init() returns success. After this function is called, the @p stream cannot be
     * used for any function call unless inflatelib_init() is called again.
     *
     * @param[in,out] stream A pointer to an #inflatelib_stream previously initialized by inflatelib_init().
     *
     * @retval #INFLATELIB_OK On success.
     * @retval #INFLATELIB_ERROR_ARG When \p stream is not initialized correctly.
     */
    INFLATELIB_EXPORT int INFLATELIB_CALLCONV inflatelib_destroy(inflatelib_stream* stream);

    /**
     * @brief Inflates DEFLATE encoded data.
     *
     * Reads a block of DEFLATE encoded data from `stream.next_in`/`stream.avail_in` and writes decoded data to
     * `stream.next_out`/`stream.avail_out`.
     *
     * Calls to this function will return #INFLATELIB_OK until all the data in the stream have been fully processed. To process
     * all the data in a stream, you should call this function in a loop until it returns #INFLATELIB_EOF (or an error).
     *
     * @param[in,out] stream A pointer to an #inflatelib_stream either in the "initialized" or "Deflate" state.
     *
     * @retval #INFLATELIB_OK When the last block of data has not been fully processed.
     * @retval #INFLATELIB_EOF When the last block of data has been fully processed.
     * @retval #INFLATELIB_ERROR_ARG When \p stream is not initialized correctly.
     * @retval #INFLATELIB_ERROR_DATA When there is an error in the input data.
     */
    INFLATELIB_EXPORT int INFLATELIB_CALLCONV inflatelib_inflate(inflatelib_stream* stream);

    /**
     * @brief Inflates DEFLATE64 encoded data.
     *
     * Reads a block of DEFLATE64 encoded data from `stream.next_in`/`stream.avail_in` and writes decoded data to
     * `stream.next_out`/`stream.avail_out`.
     *
     * Calls to this function will return #INFLATELIB_OK until all the data in the stream have been fully processed. To process
     * all the data in a stream, you should call this function in a loop until it returns #INFLATELIB_EOF (or an error).
     *
     * @param[in,out] stream A pointer to an #inflatelib_stream either in the "initialized" or "Deflate64" state.
     *
     * @retval #INFLATELIB_OK When the last block of data has not been fully processed.
     * @retval #INFLATELIB_EOF When the last block of data has been fully processed.
     * @retval #INFLATELIB_ERROR_ARG When \p stream is not initialized correctly.
     * @retval #INFLATELIB_ERROR_DATA When there is an error in the input data.
     */
    INFLATELIB_EXPORT int INFLATELIB_CALLCONV inflatelib_inflate64(inflatelib_stream* stream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INFLATELIB_H
