
#include <assert.h>
#include <string.h>

#include "bitstream.h"
#include "internal.h"

int partial_data_consistency_check(bitstream* stream)
{
    uint32_t mask = ((uint32_t)1 << stream->partial_data_size) - 1;
    return (stream->partial_data & ~mask) == 0;
}

void bitstream_init(bitstream* stream)
{
    /* Currently, there's no difference between "init" and "reset" */
    bitstream_reset(stream);
}

void bitstream_reset(bitstream* stream)
{
    stream->data = 0;
    stream->length = 0;
    stream->partial_data = 0;
    stream->partial_data_size = 0;
}

void bitstream_set_data(bitstream* stream, const uint8_t* data, size_t length)
{
    /* Should have consumed all data before calling */
    assert(stream->length == 0);

    stream->data = data;
    stream->length = length;
    /* Don't touch buffer; it may have valid data */
}

const uint8_t* bitstream_clear_data(bitstream* stream, size_t* length)
{
    const uint8_t* result = stream->data;
    *length = stream->length;

    stream->data = NULL;
    stream->length = 0;

    return result;
}

void bitstream_byte_align(bitstream* stream)
{
    /* NOTE: The only time 'partial_data' should hold more than a byte is when a previous operation failed because of
     * not enough data. In that case, we should re-try the same operation, which should consume any extra data that may
     * have temporarily been written to 'partial_data' that overflows a byte */
    assert(stream->partial_data_size < 8);
    stream->partial_data = 0;
    stream->partial_data_size = 0;
}

size_t bitstream_copy_bytes(bitstream* stream, size_t bytesToRead, uint8_t* dest)
{
    /* The caller should ensure that the stream is byte-aligned before calling this function. Because we don't put data
     * into the partial buffer unless it's (1) a partially read byte, or (2) a result of not having enough input data
     * for an operation, its size should always be zero in this code path */
    assert(stream->partial_data_size == 0);
    assert(bytesToRead > 0);

    bytesToRead = (bytesToRead > stream->length) ? stream->length : bytesToRead;
    memcpy(dest, stream->data, bytesToRead);

    stream->data += bytesToRead;
    stream->length -= bytesToRead;

    return bytesToRead;
}

size_t bitstream_read_bits(bitstream* stream, size_t bitsToRead, uint16_t* result)
{
    uint32_t data = 0, mask;
    size_t bytesNeeded;

    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(*result) * 8)));

    /* NOTE: The only scenario where 'partial_data_size' will be 1+ bytes is when a previous operation failed due to not
     * having enough input data. The next call should attempt to read the same amount of data and therefore the below
     * equation should never result in an underflow. */
    assert((bitsToRead >= stream->partial_data_size) || (stream->partial_data_size < 8));
    bytesNeeded = (bitsToRead + 7 - stream->partial_data_size) / 8;
    if (stream->length < bytesNeeded)
    {
        return 0; /* Not enough data */
    }

    switch (bytesNeeded)
    {
    default:
        assert(0);
        INFLATELIB_UNREACHABLE();

    case 2:
        data = (uint32_t)stream->data[0] | ((uint32_t)stream->data[1] << 8);
        stream->data += 2;
        stream->length -= 2;
        break;

    case 1:
        data = (uint32_t)stream->data[0];
        stream->data += 1;
        stream->length -= 1;
        break;

    case 0:
        break;
    }

    data = (data << stream->partial_data_size) | stream->partial_data;

    mask = ((uint32_t)1 << bitsToRead) - 1;
    *result = (uint16_t)(data & mask);

    stream->partial_data = data >> bitsToRead;
    stream->partial_data_size = (stream->partial_data_size + (8 * bytesNeeded)) - bitsToRead;
    assert(stream->partial_data_size < 8); /* This wasn't a failed read; should be less than a byte */
    assert(partial_data_consistency_check(stream)); /* Leading bits should be zero */

    return 1;
}

uint16_t bitstream_read_bits_unchecked(bitstream* stream, size_t bitsToRead)
{
    /* TODO: Probably remove this */
    uint32_t data = 0, mask;
    size_t bytesNeeded;

    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(uint16_t) * 8)));
    assert(stream->length >= 2); /* Caller should have checked */

    /* NOTE: See above */
    assert((bitsToRead >= stream->partial_data_size) || (stream->partial_data_size < 8));
    bytesNeeded = (bitsToRead + 7 - stream->partial_data_size) / 8;
    assert(bytesNeeded <= 2);

    data = (uint32_t)stream->data[0] | ((uint32_t)stream->data[1] << 8);
    stream->data += bytesNeeded;
    stream->length -= bytesNeeded;

    data = (data << stream->partial_data_size) | stream->partial_data;

    stream->partial_data_size = (stream->partial_data_size + (8 * bytesNeeded)) - bitsToRead;
    stream->partial_data = (data >> bitsToRead) & ((1u << stream->partial_data_size) - 1);
    assert(stream->partial_data_size < 8); /* This wasn't a failed read; should be less than a byte */
    assert(partial_data_consistency_check(stream)); /* Leading bits should be zero */

    mask = (1u << bitsToRead) - 1;
    return (uint16_t)(data & mask);
}

size_t bitstream_peek(bitstream* stream, uint16_t* result)
{
    uint32_t data = 0;
    size_t bitCount = stream->partial_data_size;

    if (stream->length >= 1)
    {
        data = (uint32_t)stream->data[0];
        if (stream->length >= 2)
        {
            data |= ((uint32_t)stream->data[1] << 8);
            bitCount = 16; /* Will always clamp to 16*/
        }
        else
        {
            bitCount = (bitCount >= 8) ? 16 : (bitCount + 8); /* Clamp to max of 16 */
        }
    }

    data = (data << stream->partial_data_size) | stream->partial_data;

    *result = (uint16_t)data;
    return bitCount;
}

uint16_t bitstream_peek_unchecked(bitstream* stream)
{
    uint32_t data;

    assert(stream->length >= 2); /* Caller should have checked */
    data = (uint32_t)stream->data[0] | ((uint32_t)stream->data[1] << 8);
    data = (data << stream->partial_data_size) | stream->partial_data;

    return (uint16_t)data;
}

void bitstream_cache_input(bitstream* stream)
{
    assert(stream->length <= 1); /* Otherwise we have enough data for any operation that can be requested of us */
    if (stream->length > 0)
    {
        stream->partial_data |= (uint32_t)stream->data[0] << stream->partial_data_size;
        stream->partial_data_size += 8;
        assert(stream->partial_data_size < 16); /* Similar assert as above; otherwise we have enough data for any operation */
        assert(partial_data_consistency_check(stream)); /* Leading bits should be zero */

        ++stream->data;
        --stream->length;
    }
}
