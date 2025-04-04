
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

static void* inflatelib_default_alloc(void* unusedUserData, size_t bytes)
{
    return malloc(bytes);
}

static void inflatelib_default_free(void* unusedUserData, void* ptr)
{
    free(ptr);
}

int inflatelib_init(inflatelib_stream* stream)
{
    int result;
    inflatelib_state* state;

    /* Start with no error message, in case it was set before (or contains uninitialized memory) */
    stream->error_msg = NULL;

    /* Setup allocation functions */
    if (stream->alloc == NULL)
    {
        stream->alloc = inflatelib_default_alloc;
    }
    if (stream->free == NULL)
    {
        stream->free = inflatelib_default_free;
    }

    /* Setup our internal state */
    state = INFLATELIB_ALLOC(stream, inflatelib_state, 1);
    if (state == NULL)
    {
        stream->error_msg = "Failed to allocate storage for internal state";
        errno = ENOMEM;
        return INFLATELIB_ERROR_OOM;
    }

    memset(state, 0, sizeof(*state));
    stream->internal = state;

    result = huffman_tree_init(&state->code_length_tree, stream, CODE_LENGTH_TREE_ELEMENT_COUNT);
    if (result >= 0)
    {
        result = huffman_tree_init(&state->literal_length_tree, stream, LITERAL_TREE_MAX_ELEMENT_COUNT);
    }
    if (result >= 0)
    {
        result = huffman_tree_init(&state->distance_tree, stream, DIST_TREE_MAX_ELEMENT_COUNT);
    }

    if (result >= 0)
    {
        bitstream_init(&state->bitstream);
        window_init(&state->window);

        state->ifstate = ifstate_reading_bfinal;
    }

    if (result < 0)
    {
        /* This will take care of deallocating any allocated memory */
        inflatelib_destroy(stream);
        return result;
    }

    return INFLATELIB_OK;
}

int inflatelib_reset(inflatelib_stream* stream)
{
    inflatelib_state* state = stream->internal;

    if (state == NULL)
    {
        stream->error_msg = "Internal state is null; ensure inflatelib_init has been called first";
        errno = EINVAL;
        return INFLATELIB_ERROR_ARG;
    }

    bitstream_reset(&state->bitstream);
    window_reset(&state->window);

    // NOTE: The Huffman trees do not need to be reset as they are reset on demand as needed. If we've made it this far,
    // all of their internal state has been allocated, and that's the best that we can ask for

    state->ifstate = ifstate_reading_bfinal;

    return INFLATELIB_OK;
}

int inflatelib_destroy(inflatelib_stream* stream)
{
    inflatelib_state* state = stream->internal;

    /* NOTE: It should not be possible for the pointer to be non-null unless alloc/free were initialized, at least so
             long as the caller zero-initialized the pointer */
    if (state)
    {
        if (state->error_msg_fmt)
        {
            if (stream->error_msg == state->error_msg_fmt)
            {
                /* Don't leave a dangling pointer, however also don't fully null out the error message in case the
                   caller is depending on being able to read it. Realistically, this should never cause problems in
                   practice as we should never need to format an error message AND destroy the stream in one op */
                stream->error_msg = "Generic failure";
            }

            INFLATELIB_FREE(stream, state->error_msg_fmt);
        }

        INFLATELIB_FREE(stream, stream->internal);
        stream->internal = NULL;
    }

    return INFLATELIB_OK;
}

int format_error_message(inflatelib_stream* stream, const char* fmt, ...)
{
    va_list args;
    int bufferSize;
    inflatelib_state* state = stream->internal;

    /* Before starting, clear out the previously allocated error message, if present */
    if (state->error_msg_fmt)
    {
        if (stream->error_msg == state->error_msg_fmt)
        {
            stream->error_msg = NULL;
        }

        INFLATELIB_FREE(stream, state->error_msg_fmt);
        state->error_msg_fmt = NULL;
    }

    va_start(args, fmt);
    bufferSize = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (bufferSize < 0)
    {
        stream->error_msg = "Failed to format error message";
        return INFLATELIB_ERROR_ARG;
    }

    ++bufferSize; /* Return value does not include space for null terminator */

    state->error_msg_fmt = INFLATELIB_ALLOC(stream, char, bufferSize);
    if (!state->error_msg_fmt)
    {
        stream->error_msg = "Failed to allocate space for error message";
        errno = ENOMEM;
        return INFLATELIB_ERROR_OOM;
    }

    va_start(args, fmt);
    bufferSize = vsnprintf(state->error_msg_fmt, bufferSize, fmt, args);
    va_end(args);

    if (bufferSize < 0)
    {
        stream->error_msg = "Failed to format error message";
        return INFLATELIB_ERROR_ARG;
    }

    stream->error_msg = state->error_msg_fmt;
    return INFLATELIB_OK;
}

static int inflate64_process_data(inflatelib_stream* stream);
static int inflate64_read_uncompressed(inflatelib_stream* stream);
static void inflate64_init_static_tables(inflatelib_stream* stream);
static int inflate64_read_dynamic_header(inflatelib_stream* stream);
static int inflate64_read_compressed(inflatelib_stream* stream);

int inflatelib_inflate64(inflatelib_stream* stream)
{
    int result;
    inflatelib_state* state = stream->internal;
    size_t initialOutSize = stream->avail_out;

    if (state == NULL)
    {
        stream->error_msg = "Internal state is null; ensure inflatelib_init has been called first";
        errno = EINVAL;
        return INFLATELIB_ERROR_ARG;
    }

    /* The last call to inflatelib_inflate64 may not have read all data, e.g. if we've filled up the output buffer,
     * however we should have reset the buffer to avoid the dangling pointer */
    bitstream_set_data(&state->bitstream, (const uint8_t*)stream->next_in, stream->avail_in);

    result = inflate64_process_data(stream);

    /* When making it this far, we've potentially read/written data that we want to report, even on failure */
    stream->total_out += initialOutSize - stream->avail_out;

    stream->total_in += stream->avail_in - state->bitstream.length;
    stream->next_in = state->bitstream.data;
    stream->avail_in = state->bitstream.length;
    state->bitstream.length = 0; /* NOTE: This is enough to ensure we don't read from 'data' */

    return result;
}

static int inflate64_process_data(inflatelib_stream* stream)
{
    inflatelib_state* state = stream->internal;
    int result;
    uint16_t data;

    do
    {
        switch (state->ifstate)
        {
        case ifstate_reading_bfinal:
            if (!bitstream_read_bits(&state->bitstream, 1, &data))
            {
                return INFLATELIB_OK; /* Not enough data */
            }

            state->bfinal = (int)data;
            state->ifstate = ifstate_reading_btype;
            /* Fallthrough */

        case ifstate_reading_btype:
            if (!bitstream_read_bits(&state->bitstream, 2, &data))
            {
                return INFLATELIB_OK; /* Not enough data */
            }
            else if (data > 2)
            {
                if (format_error_message(stream, "Unexpected block type '%u'", data) < 0)
                {
                    stream->error_msg = "Unexpected block type";
                }
                errno = EINVAL;
                return INFLATELIB_ERROR_DATA;
            }

            state->btype = (block_type)data;
            switch (state->btype)
            {
            case btype_uncompressed:
                bitstream_byte_align(&state->bitstream);
                state->ifstate = ifstate_reading_uncompressed_block_len;
                break;

            case btype_static:
                inflate64_init_static_tables(stream);
                state->ifstate = ifstate_reading_literal_length_code;
                break;

            case btype_dynamic:
                state->ifstate = ifstate_reading_num_lit_codes;
                break;
            }
            break; /* Handled below */

        case ifstate_eof:
            return INFLATELIB_EOF; /* Already read all data */

        default:
            /* Otherwise, 'btype' is known & we're in the process of decoding; handled below */
            break;
        }

        switch (state->btype)
        {
        case btype_uncompressed:
            result = inflate64_read_uncompressed(stream);
            break;

        default:
            assert(0); /* Otherwise invalid block_type */
        case btype_dynamic:
            if (state->ifstate < ifstate_reading_literal_length_code)
            {
                /* We have not fully initialized the dynamic Huffman tables yet */
                result = inflate64_read_dynamic_header(stream);
                if (result < 0)
                {
                    return result;
                }

                /* We'll return '0' to indicate success, however this could also mean that there was not enough data to
                 * finish initializing the Huffman trees */
                if (state->ifstate < ifstate_reading_literal_length_code)
                {
                    return INFLATELIB_OK; /* Not enough data */
                }
            }
            /* Fallthrough */

        case btype_static:
            result = inflate64_read_compressed(stream);
            break;
        }
    } while ((result == INFLATELIB_OK) && (state->ifstate == ifstate_reading_bfinal));

    if ((result == INFLATELIB_OK) && (state->ifstate == ifstate_eof))
    {
        result = INFLATELIB_EOF;
    }

    return result;
}

static int inflate64_read_uncompressed(inflatelib_stream* stream)
{
    inflatelib_state* state = stream->internal;
    int result = INFLATELIB_OK;
    size_t bytesCopied;
    uint16_t data;

    assert(state->btype == btype_uncompressed);

    switch (state->ifstate)
    {
    case ifstate_reading_uncompressed_block_len:
        if (!bitstream_read_bits(&state->bitstream, 16, &data))
        {
            return INFLATELIB_OK; /* Not enough data */
        }

        state->data.uncompressed.block_len = data;
        state->ifstate = ifstate_reading_uncompressed_block_len_complement;
        /* Fallthrough */

    case ifstate_reading_uncompressed_block_len_complement:
        if (!bitstream_read_bits(&state->bitstream, 16, &data))
        {
            return INFLATELIB_OK; /* Not enough data */
        }

        if (state->data.uncompressed.block_len != (uint16_t)~data)
        {
            if (format_error_message(
                    stream,
                    "Uncompressed block length (%04X) does not match its encoded one's complement value (%04X)",
                    state->data.uncompressed.block_len,
                    data) < 0)
            {
                stream->error_msg = "Uncompressed block length does not match its encoded one's complement value";
            }
            errno = EINVAL;
            return INFLATELIB_ERROR_DATA;
        }

        state->ifstate = ifstate_reading_uncompressed_data;
        /* Fallthrough */

    case ifstate_reading_uncompressed_data:
        /* NOTE: Both these function calls are safe to call with sizes of zero */
        state->data.uncompressed.block_len -= window_copy_bytes(&state->window, &state->bitstream, state->data.uncompressed.block_len);

        bytesCopied = window_copy_output(&state->window, (uint8_t*)stream->next_out, stream->avail_out);
        stream->next_out = (uint8_t*)stream->next_out + bytesCopied;
        stream->avail_out -= bytesCopied;

        /* It's safe to continue only if we've read and written all data */
        if ((state->data.uncompressed.block_len == 0) && (state->window.unconsumed_bytes == 0))
        {
            state->ifstate = state->bfinal ? ifstate_eof : ifstate_reading_bfinal;
        }
        break;

    default:
        assert(0); /* Invalid state for 'btype_uncompressed' */
    }

    return INFLATELIB_OK;
}

static void inflate64_init_static_tables(inflatelib_stream* stream)
{
    int result;
    uint8_t buffer[LITERAL_TREE_MAX_ELEMENT_COUNT];
    inflatelib_state* state = stream->internal;

    /* TODO: We can encode both of these tables in static data; it's not clear yet if/how much that might improve things */

    /*
     * The static literal/length code lengths are specified by RFC 1951, section 3.2.6 as follows:
     *      0-143: 8 bits long
     *      144-255: 9 bits long
     *      256-279: 7 bits long
     *      280-287: 8 bits long
     */
    for (size_t i = 0; i < 144; ++i)
    {
        buffer[i] = 8;
    }
    for (size_t i = 144; i < 256; ++i)
    {
        buffer[i] = 9;
    }
    for (size_t i = 256; i < 280; ++i)
    {
        buffer[i] = 7;
    }
    for (size_t i = 280; i < 288; ++i)
    {
        buffer[i] = 8;
    }

    result = huffman_tree_reset(&state->literal_length_tree, stream, buffer, 288);
    assert(result == 0); /* We control the inputs; this can never fail */

    /* The distance code lengths are also specified by RFC 1951, section 3.2.6 as being 5 bits each */
    for (size_t i = 0; i < 32; ++i)
    {
        buffer[i] = 5;
    }

    result = huffman_tree_reset(&state->distance_tree, stream, buffer, 32);
    assert(result == 0); /* We control the inputs; this can never fail */
}

/* The order that the code length alphabe's code lengths are specified in, as per RFC 1951, section 3.2.7 */
static const uint8_t code_order[CODE_LENGTH_TREE_ELEMENT_COUNT] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static int inflate64_read_dynamic_header(inflatelib_stream* stream)
{
    int result = INFLATELIB_OK;
    inflatelib_state* state = stream->internal;
    uint16_t data, prevCode, codeArraySize;

    assert(state->btype == btype_dynamic);

    switch (state->ifstate)
    {
    case ifstate_reading_num_lit_codes:
        if (!bitstream_read_bits(&state->bitstream, 5, &data))
        {
            return INFLATELIB_OK; /* Not enough data */
        }
        state->data.dynamic_codes.literal_length_code_count = data + 257;
        state->ifstate = ifstate_reading_num_dist_codes;
        /* Fallthrough */

    case ifstate_reading_num_dist_codes:
        if (!bitstream_read_bits(&state->bitstream, 5, &data))
        {
            return INFLATELIB_OK; /* Not enough data */
        }
        state->data.dynamic_codes.distance_code_count = (uint8_t)(data + 1);
        state->ifstate = ifstate_reading_num_code_len_codes;
        /* Fallthrough */

    case ifstate_reading_num_code_len_codes:
        if (!bitstream_read_bits(&state->bitstream, 4, &data))
        {
            return INFLATELIB_OK; /* Not enough data */
        }
        state->data.dynamic_codes.code_length_code_count = (uint8_t)(data + 4);
        state->data.dynamic_codes.loop_counter = 0;
        state->ifstate = ifstate_reading_code_len_codes;
        /* Fallthrough */

    case ifstate_reading_code_len_codes:
        /* NOTE: Should be impossible since we read 4 bits (0-15) then add 4 */
        assert(state->data.dynamic_codes.code_length_code_count <= CODE_LENGTH_TREE_ELEMENT_COUNT);
        while (state->data.dynamic_codes.loop_counter < state->data.dynamic_codes.code_length_code_count)
        {
            if (!bitstream_read_bits(&state->bitstream, 3, &data))
            {
                return INFLATELIB_OK; /* Not enough data */
            }

            state->data.dynamic_codes.code_lengths[code_order[state->data.dynamic_codes.loop_counter]] = (uint8_t)data;
            ++state->data.dynamic_codes.loop_counter;
        }

        /* Fill rest of the array with zeroes */
        while (state->data.dynamic_codes.loop_counter < inflatelib_arraysize(code_order))
        {
            state->data.dynamic_codes.code_lengths[code_order[state->data.dynamic_codes.loop_counter]] = 0;
            ++state->data.dynamic_codes.loop_counter;
        }

        result = huffman_tree_reset(&state->code_length_tree, stream, state->data.dynamic_codes.code_lengths, CODE_LENGTH_TREE_ELEMENT_COUNT);
        if (result < 0)
        {
            return result; /* Error message, etc. already set */
        }

        state->data.dynamic_codes.loop_counter = 0; /* Reset for next operation */
        state->ifstate = ifstate_reading_tree_codes_before;
        /* Fallthrough */

    case ifstate_reading_tree_codes_before:
    case ifstate_reading_tree_codes_after:
        codeArraySize = state->data.dynamic_codes.literal_length_code_count + state->data.dynamic_codes.distance_code_count;
        assert(codeArraySize <= inflatelib_arraysize(state->data.dynamic_codes.code_lengths));
        while (state->data.dynamic_codes.loop_counter < codeArraySize)
        {
            if (state->ifstate == ifstate_reading_tree_codes_before)
            {
                result = huffman_tree_lookup(&state->code_length_tree, stream, &data);
                if (!result)
                {
                    return INFLATELIB_OK; /* Not enough data */
                }
                else if (result < 0)
                {
                    return INFLATELIB_ERROR_DATA; /* Error message, etc. already set */
                }

                state->data.dynamic_codes.length_code = (uint8_t)data;
            }

            /* Decode values from the code length array, as specified by RFC 1951, section 3.2.7 */
            if (state->data.dynamic_codes.length_code <= 15)
            {
                /* Literal value */
                state->data.dynamic_codes.code_lengths[state->data.dynamic_codes.loop_counter++] = state->data.dynamic_codes.length_code;
            }
            else if (state->data.dynamic_codes.length_code == 16)
            {
                /* Repeat the previous code length 3-6 times as specified by the next two bits */
                if (!bitstream_read_bits(&state->bitstream, 2, &data))
                {
                    /* Not enough data; ensure we don't read a new length code next time */
                    state->ifstate = ifstate_reading_tree_codes_after;
                    return INFLATELIB_OK;
                }

                if (state->data.dynamic_codes.loop_counter == 0)
                {
                    stream->error_msg = "Code length repeat code encountered at beginning of data";
                    errno = EINVAL;
                    return INFLATELIB_ERROR_DATA;
                }
                prevCode = state->data.dynamic_codes.code_lengths[state->data.dynamic_codes.loop_counter - 1];

                data += 3;
                if ((state->data.dynamic_codes.loop_counter + data) > codeArraySize)
                {
                    if (format_error_message(
                            stream,
                            "Code length repeat code specifies %u repetitions, but only %u codes remain",
                            data,
                            codeArraySize - state->data.dynamic_codes.loop_counter) < 0)
                    {
                        stream->error_msg = "Code length repeat code specifies more repetitions than codes remain";
                    }
                    errno = EINVAL;
                    return INFLATELIB_ERROR_DATA;
                }

                for (uint16_t i = 0; i < data; ++i)
                {
                    state->data.dynamic_codes.code_lengths[state->data.dynamic_codes.loop_counter++] = prevCode;
                }
            }
            else
            {
                /* Repeat zero some number of times */
                int bitCount;
                uint16_t repeatBase;
                if (state->data.dynamic_codes.length_code == 17)
                {
                    /* Repeat '0' 3-10 times as specified by the next 3 bits */
                    bitCount = 3;
                    repeatBase = 3;
                }
                else
                {
                    /* Repeat '0' 11-138 times*/
                    assert(state->data.dynamic_codes.length_code == 18);
                    bitCount = 7;
                    repeatBase = 11;
                }

                if (!bitstream_read_bits(&state->bitstream, bitCount, &data))
                {
                    /* Not enough data; ensure we don't read a new length code next time */
                    state->ifstate = ifstate_reading_tree_codes_after;
                    return INFLATELIB_OK;
                }

                data += repeatBase;
                if ((state->data.dynamic_codes.loop_counter + data) > codeArraySize)
                {
                    if (format_error_message(
                            stream,
                            "Zero repeat code specifies %u repetitions, but only %u codes remain",
                            data,
                            codeArraySize - state->data.dynamic_codes.loop_counter) < 0)
                    {
                        stream->error_msg = "Zero repeat code specifies more repetitions than codes remain";
                    }
                    errno = EINVAL;
                    return INFLATELIB_ERROR_DATA;
                }

                for (uint16_t i = 0; i < data; ++i)
                {
                    state->data.dynamic_codes.code_lengths[state->data.dynamic_codes.loop_counter++] = 0;
                }
            }

            /* If we get this far, it means that we're done with the current code and ready for the next one */
            state->ifstate = ifstate_reading_tree_codes_before;
        }

        /* If we break out of the loop, we're done reading the code lengths arrays and are ready to init & move on */
        result = huffman_tree_reset(
            &state->literal_length_tree, stream, state->data.dynamic_codes.code_lengths, state->data.dynamic_codes.literal_length_code_count);
        if (result < 0)
        {
            return result; /* Error message, etc. already set */
        }

        result = huffman_tree_reset(
            &state->distance_tree,
            stream,
            state->data.dynamic_codes.code_lengths + state->data.dynamic_codes.literal_length_code_count,
            state->data.dynamic_codes.distance_code_count);
        if (result < 0)
        {
            return result; /* Error message, etc. already set */
        }

        state->ifstate = ifstate_reading_literal_length_code;
        break;

    default:
        assert(0); /* Otherwise should not have called this function */
        break;
    }

    return INFLATELIB_OK;
}

/* The data for reading encoded lengths. For some symbol N, N >= 257, the length of the block is:
 * length_base[N - 257] + bitstream_read_bits(..., length_extra_bits[N - 257]) */
/* NOTE: The primary difference between Deflate and Deflate64 w.r.t. the length encoding is that the final entry (the
 * entry for symbol 285) has a base/extra bits of 258/0 for Deflate, whereas it's 3/16 for Deflate64 */
static const uint16_t length_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                         31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 3};
static const uint16_t length_extra_bits[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 16};

/* The data for reading encoded distances. For some symbol N, 0 <= N <= 31, the distance is:
 * distance_base[N] + bitstream_read_bits(..., distance_extra_bits[N]) */
/* NOTE: The only difference between Deflate and Deflate64 w.r.t. the distance encoding is that Deflate64 makes use of
 * symbols 30 and 31 */
/* TODO: Another way to calculate the number of extra bits is (N - 2) >> 1; see which is better */
static const uint16_t distance_base[32] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,   33,
                                           49,   65,   97,   129,  193,  257,   385,   513,   769,   1025, 1537,
                                           2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153};
static const uint16_t distance_extra_bits[32] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
                                                 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

static int inflate64_read_compressed(inflatelib_stream* stream)
{
    int result = INFLATELIB_OK;
    inflatelib_state* state = stream->internal;
    uint8_t* out = (uint8_t*)stream->next_out;
    size_t bytesCopied, outSize = stream->avail_out;
    uint16_t symbol;
    int opResult, keepGoing = 1;

    while (keepGoing)
    {
        switch (state->ifstate)
        {
        case ifstate_reading_literal_length_code:
            /* We're in the process of reading a value from the literal/length tree */
            opResult = huffman_tree_lookup(&state->literal_length_tree, stream, &state->data.compressed.symbol);
            if (opResult == 0)
            {
                keepGoing = 0; /* Not enough data in the input */
                break;
            }
            else if (opResult < 0)
            {
                /* Error in the data; NOTE: We've already set the error message */
                keepGoing = 0;
                result = INFLATELIB_ERROR_DATA;
                break;
            }
            /* Fallthrough */

        case ifstate_decoding_literal_length_code:
            if (state->data.compressed.symbol < 256) /* Literal */
            {
                if (!window_write_byte(&state->window, (uint8_t)state->data.compressed.symbol))
                {
                    /* Not enough data in the window; try and read some data to free up space */
                    if (!window_copy_output(&state->window, out, outSize))
                    {
                        keepGoing = 0; /* Not enough data in the output */
                        state->ifstate = ifstate_decoding_literal_length_code;
                        break;
                    }

                    /* Otherwise, we copyied at least one byte and therefore this write should succeed */
                    opResult = window_write_byte(&state->window, (uint8_t)state->data.compressed.symbol);
                    assert(opResult);
                }

                state->ifstate = ifstate_reading_literal_length_code;
                break;
            }
            else if (state->data.compressed.symbol == 256) /* End of block */
            {
                state->ifstate = ifstate_copying_output_from_window;
                break;
            }
            else if (state->data.compressed.symbol > 285)
            {
                /* NOTE: HLIT is 5 bits, which means that there are at most 288 code lengths specified for the
                 * literal/length tree (257 + 31). This means that in theory, someone could author a block where symbols
                 * can go from 0 to 287. If we move this error "up" and error out if HLIT is greater than 29, we can
                 * eliminate this error check, which could potentially give us some perf wins at the cost of potentially
                 * rejecting otherwise valid inputs. */
                if (format_error_message(stream, "Invalid symbol '%u' from literal/length tree", state->data.compressed.symbol) < 0)
                {
                    stream->error_msg = "Invalid symbol from literal/length tree";
                }
                keepGoing = 0;
                errno = EINVAL;
                result = INFLATELIB_ERROR_DATA;
                break;
            }

            /* Otherwise, 'symbol' references a length */
            symbol = state->data.compressed.symbol - 257;
            assert(symbol < inflatelib_arraysize(length_base)); /* Shouldn't have passed check above */
            state->data.compressed.block_length = length_base[symbol];
            state->data.compressed.extra_bits = length_extra_bits[symbol];
            /* Fallthrough */

        case ifstate_reading_length_extra_bits:
            if (state->data.compressed.extra_bits > 0)
            {
                if (!bitstream_read_bits(&state->bitstream, state->data.compressed.extra_bits, &symbol))
                {
                    keepGoing = 0; /* Not enough data in the input */
                    state->ifstate = ifstate_reading_length_extra_bits;
                    break;
                }

                state->data.compressed.block_length += symbol;
            }
            /* Fallthrough */

        case ifstate_reading_distance_code:
            /* Now we need to read a distance */
            opResult = huffman_tree_lookup(&state->distance_tree, stream, &symbol);
            if (opResult == 0)
            {
                keepGoing = 0; /* Not enough data in the input */
                state->ifstate = ifstate_reading_distance_code;
                break;
            }
            else if (opResult < 0)
            {
                /* Error in the data; NOTE: We've already set the error message */
                keepGoing = 0;
                result = INFLATELIB_ERROR_DATA;
                break;
            }

            /* NOTE: HDIST is 5 bits, giving a maximum of 32 distance symbols, the exact size of the table */
            assert(symbol < inflatelib_arraysize(distance_base));
            state->data.compressed.block_distance = distance_base[symbol];
            state->data.compressed.extra_bits = distance_extra_bits[symbol];
            /* Fallthrough */

        case ifstate_reading_distance_extra_bits:
            if (state->data.compressed.extra_bits > 0)
            {
                if (!bitstream_read_bits(&state->bitstream, state->data.compressed.extra_bits, &symbol))
                {
                    keepGoing = 0; /* Not enough data in the input */
                    state->ifstate = ifstate_reading_distance_extra_bits;
                    break;
                }

                state->data.compressed.block_distance += symbol;
            }
            /* Fallthrough */

            /* NOTE: It's not guaranteed we have enough space available in 'out' to write all data, hence the need for a
             * dedicated state for copying the data from the window */
        case ifstate_copying_length_distance_from_window:
            opResult =
                window_copy_length_distance(&state->window, state->data.compressed.block_distance, state->data.compressed.block_length);
            if (opResult < 0)
            {
                keepGoing = 0;
                if (format_error_message(
                        stream,
                        "Compressed block has a distance '%u' which exceeds the size of the window (%llu bytes)",
                        state->data.compressed.block_distance,
                        state->window.total_bytes) < 0)
                {
                    stream->error_msg = "Compressed block has a distance which exceeds the size of the window";
                }
                errno = EINVAL;
                result = INFLATELIB_ERROR_DATA;
                break;
            }

            state->data.compressed.block_length -= (uint32_t)opResult;

            bytesCopied = window_copy_output(&state->window, out, outSize);
            out += bytesCopied;
            outSize -= bytesCopied;

            /* There are two scenarios where the operation is not yet complete at this point: (1) 'block_length' was too
             * long to copy all data in a single operation, or (2) we ran out of space in the output buffer */
            if ((state->data.compressed.block_length == 0) && (state->window.unconsumed_bytes == 0))
            {
                /* Repeat the process until we hit the end of block symbol (256) */
                state->ifstate = ifstate_reading_literal_length_code;
            }
            else
            {
                state->ifstate = ifstate_copying_length_distance_from_window;
                assert((state->data.compressed.block_length != 0) || (outSize == 0));

                if (((state->data.compressed.block_length == 0) || (opResult == 0)) && (outSize == 0))
                {
                    /* Can't copy any more data in the window and can't copy any more data to the output... need to
                     * return to the caller so they can give us a larger output buffer to write to */
                    keepGoing = 0;
                }
            }
            break;

        case ifstate_copying_output_from_window:
            /* This state means we've read all input; we just need to finish copying data to the output */
            bytesCopied = window_copy_output(&state->window, out, outSize);
            out += bytesCopied;
            outSize -= bytesCopied;
            if (state->window.unconsumed_bytes == 0)
            {
                /* All data consumed; go back to reading bfinal */
                state->ifstate = state->bfinal ? ifstate_eof : ifstate_reading_bfinal;
            }

            /* Even if we're not done reading bytes, we've run out of space in the output and need to return */
            keepGoing = 0;
            break;

        default:
            assert(0); /* Should not be evaluating this function then */
            break;
        }
    }

    /* Copy as much data from the window as we can before returning */
    bytesCopied = window_copy_output(&state->window, out, outSize);
    out += bytesCopied;
    outSize -= bytesCopied;

    /* Update the output buffers to reflect what we wrote */
    stream->next_out = out;
    stream->avail_out = outSize;

    return result;
}
