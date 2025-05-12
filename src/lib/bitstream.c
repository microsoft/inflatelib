
#include <assert.h>
#include <string.h>

#include "bitstream.h"

void bitstream_init(bitstream* stream)
{
    /* Currently, there's no difference between "init" and "reset" */
    bitstream_reset(stream);
}

void bitstream_reset(bitstream* stream)
{
    stream->data = 0;
    stream->length = 0;
    stream->bits_consumed = 0;
}

void bitstream_set_data(bitstream* stream, const uint8_t* data, size_t length)
{
    /* Should have consumed all data before calling */
    assert(stream->length == 0);

    stream->data = data;
    stream->length = length;
    /* Don't touch buffer; it may have valid data */
}

void bitstream_byte_align(bitstream* stream)
{
    /* It's possible to still have "extra" data in the buffer if we previously did a peek & consume prior to this
     * operation, so we can't just set 'bits_consumed' to zero */
    if (stream->bits_consumed != 0)
    {
        stream->bits_consumed = 0;
        stream->data += 1;
        stream->length -= 1;
    }
}

uint16_t bitstream_copy_bytes(bitstream* stream, uint16_t bytesToRead, uint8_t* dest)
{
    uint16_t bytesFromData;

    /* The caller should ensure that the stream is byte-aligned before calling this function. It may be the case that
     * some data is already in the buffer - e.g. if the previous operation was a peek - in which case there should be
     * a multiple of 8-bits in the buffer */
    assert(stream->bits_consumed == 0);
    assert(bytesToRead > 0);

    bytesFromData = (stream->length > bytesToRead) ? bytesToRead : (uint16_t)stream->length;

    memcpy(dest, stream->data, bytesFromData);

    stream->data += bytesFromData;
    stream->length -= bytesFromData;

    return bytesFromData;
}

size_t bitstream_read_bits(bitstream* stream, uint8_t bitsToRead, uint16_t* result)
{
    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(*result) * 8)));

    size_t bitsConsumed = stream->bits_consumed;
    size_t bitsConsumedTotal = bitsConsumed + bitsToRead;

    if (stream->length * 8 < bitsConsumedTotal)
    {
        return 0; /* Not enough data */
    }

    uint32_t v;
    memcpy(&v, stream->data, sizeof(v));

    v >>= bitsConsumed;
    v &= ((uint32_t)1 << bitsToRead) - 1;

    *result = (uint16_t)v;

    bitstream_consume_bits(stream, bitsToRead);
    return 1;
}

uint16_t bitstream_read_bits_unchecked(bitstream* stream, uint8_t bitsToRead)
{
    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(uint16_t) * 8)));

    uint32_t v;
    memcpy(&v, stream->data, sizeof(v));

    v >>= stream->bits_consumed;
    v &= ((uint32_t)1 << bitsToRead) - 1;

    bitstream_consume_bits(stream, bitsToRead);
    return (uint16_t)v;
}

size_t bitstream_peek(bitstream* stream, uint16_t* result)
{
    size_t remaining = stream->length * 8 - stream->bits_consumed;
    remaining = remaining > 16 ? 16 : remaining;

    uint32_t v;
    memcpy(&v, stream->data, sizeof(v));

    v >>= stream->bits_consumed;
    v &= ((uint32_t)1 << remaining) - 1;

    *result = (uint16_t)v;
    return remaining;
}

uint16_t bitstream_peek_unchecked(bitstream* stream)
{
    uint32_t v;
    memcpy(&v, stream->data, sizeof(v));

    v >>= stream->bits_consumed;
    v &= 0xffff;
    return (uint16_t)v;
}

void bitstream_consume_bits(bitstream* stream, size_t bits)
{
    size_t total = stream->bits_consumed + bits;
    size_t bytes = total / 8;
    assert(stream->length >= bytes);
    stream->data += bytes;
    stream->length -= bytes;
    stream->bits_consumed = total - bytes * 8;
}
