
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
    if (stream->bits_consumed != 0)
    {
        stream->bits_consumed = 0;
        ++stream->data;
        --stream->length;
    }
}

size_t bitstream_copy_bytes(bitstream* stream, size_t bytesToRead, uint8_t* dest)
{
    size_t bytesFromData;

    /* The caller should ensure that the stream is byte-aligned before calling this function. It may be the case that
     * some data is already in the buffer - e.g. if the previous operation was a peek - in which case there should be
     * a multiple of 8-bits in the buffer */
    assert(stream->bits_consumed == 0);
    assert(bytesToRead > 0);

    bytesFromData = (stream->length > bytesToRead) ? bytesToRead : stream->length;

    memcpy(dest, stream->data, bytesFromData);

    stream->data += bytesFromData;
    stream->length -= bytesFromData;

    return bytesFromData;
}

size_t bitstream_read_bits(bitstream* stream, size_t bitsToRead, uint16_t* result)
{
    size_t bitsNeeded = stream->bits_consumed + bitsToRead;
    uint32_t value = 0;

    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(*result) * 8)));

    if ((stream->length * 8) < bitsNeeded)
    {
        return 0; /* Not enough data */
    }

    memcpy(&value, stream->data, (bitsNeeded + 7) / 8);
    value >>= stream->bits_consumed;
    value &= ((uint32_t)1 << bitsToRead) - 1;

    bitstream_consume_bits(stream, bitsToRead);
    *result = (uint16_t)value;
    return 1;
}

uint16_t bitstream_read_bits_unchecked(bitstream* stream, size_t bitsToRead)
{
    uint32_t value = 0;

    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(uint16_t) * 8)));
    assert(stream->length >= sizeof(value));

    memcpy(&value, stream->data, sizeof(value));
    value >>= stream->bits_consumed;
    value &= ((uint32_t)1 << bitsToRead) - 1;

    bitstream_consume_bits(stream, bitsToRead);
    return (uint16_t)value;
}

size_t bitstream_peek(bitstream* stream, uint16_t* result)
{
    size_t bitsAvail = (stream->length < 3) ? ((stream->length * 8) - stream->bits_consumed) : 16;
    uint32_t value = 0;

    memcpy(&value, stream->data, (bitsAvail + stream->bits_consumed + 7) / 8);
    value >>= stream->bits_consumed;
    value &= ((uint32_t)1 << bitsAvail) - 1;

    *result = (uint16_t)value;
    return bitsAvail;
}

uint16_t bitstream_peek_unchecked(bitstream* stream)
{
    uint32_t value;

    assert(stream->length >= sizeof(value));

    memcpy(&value, stream->data, sizeof(value));
    value >>= stream->bits_consumed;

    return (uint16_t)value;
}

void bitstream_consume_bits(bitstream* stream, size_t bits)
{
    size_t bytesToAdvance = (bits + stream->bits_consumed) / 8;

    assert((bits + stream->bits_consumed + 7) / 8 <= stream->length);

    stream->data += bytesToAdvance;
    stream->length -= bytesToAdvance;
    stream->bits_consumed = (bits + stream->bits_consumed) % 8;
}
