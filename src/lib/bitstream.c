
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
    stream->buffer = 0;
    stream->bits_in_buffer = 0;
    stream->extra_buffer = 0;
    stream->bits_in_extra_buffer = 0;
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
    /* NOTE: The actual number of unprocessed bits is a combination of the bits in 'buffer' as well as 'extra_buffer' */
    int bitsToConsume = (stream->bits_in_buffer + stream->bits_in_extra_buffer) % 8;

    /* We read data such that we don't use the extra buffer if we can otherwise fit 64 bits in buffer */
    assert((stream->bits_in_buffer + stream->bits_in_extra_buffer - bitsToConsume) <= 64);

    stream->buffer >>= bitsToConsume;
    stream->bits_in_buffer -= bitsToConsume;
    stream->buffer |= (uint64_t)stream->extra_buffer << stream->bits_in_buffer;
    stream->bits_in_buffer += stream->bits_in_extra_buffer;
    stream->bits_in_extra_buffer = 0;
    stream->extra_buffer = 0;
}

int bitstream_copy_bytes(bitstream* stream, int bytesToRead, uint8_t* dest)
{
    int bytesFromBuffer, bytesFromData;

    /* The caller should ensure that the stream is byte-aligned before calling this function. It may be the case that
     * some data is already in the buffer - e.g. if the previous operation was a peek - in which case there should be
     * a multiple of 8-bits in the buffer */
    assert((stream->bits_in_buffer % 8) == 0);
    assert(stream->bits_in_extra_buffer == 0);
    assert(bytesToRead > 0);

    bytesFromBuffer = stream->bits_in_buffer / 8;
    bytesFromBuffer = (bytesFromBuffer > bytesToRead) ? bytesToRead : bytesFromBuffer;

    for (int i = 0; i < bytesFromBuffer; ++i)
    {
        *dest++ = (uint8_t)stream->buffer;
        stream->buffer >>= 8;
        stream->bits_in_buffer -= 8;
    }
    bytesToRead -= bytesFromBuffer;

    bytesFromData = (stream->length > (size_t)bytesToRead) ? bytesToRead : (int)stream->length;

    memcpy(dest, stream->data, (size_t)bytesFromData);

    stream->data += bytesFromData;
    stream->length -= bytesFromData;

    return bytesFromBuffer + bytesFromData;
}

static int bitstream_fill_buffer_small(bitstream* stream);

int bitstream_fill_buffer(bitstream* stream)
{
    uint64_t extraData, bufferMask, extraBufferMaskLow;
    int extraBits, bytesToRead, bitsAddedToBuffer, bitsAddedToExtraBuffer;;
    int bitsInAllBuffers = stream->bits_in_buffer + stream->bits_in_extra_buffer;

    /* We can test fewer conditionals if we split this function into two: one where we assume we are filling the entire
     * buffer and another where we assume we are consuming all of the input */
    if (((bitsInAllBuffers / 8) + stream->length) < 8)
    {
        return bitstream_fill_buffer_small(stream);
    }

    /* See comment in the header file. Our goal is to ensure that 'buffer' and 'extra_buffer' collectively have between
     * 64 and 71 bits */
    extraData = stream->extra_buffer;
    extraBits = stream->bits_in_extra_buffer;
    bytesToRead = (71 - bitsInAllBuffers) / 8;
    assert(bytesToRead <= stream->length);

    /* TODO: Test doing this in reverse; allows us to skip incrementing both data and extraBits in the switch */
    switch (bytesToRead)
    {
    case 8:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 7:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 6:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 5:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 4:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 3:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 2:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 1:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 0:
        break;
    default:
        /* TODO: Unreachable */
        assert(0); /* Should never happen */
        break;
    }

    stream->length -= bytesToRead;
    assert((stream->bits_in_buffer + extraBits) < 72); /* Sanity check */

    /* NOTE: There are two somewhat interesting situations, both of which are valid: (1) calling this function when
     * 'buffer' is empty, and (2) calling this function when 'buffer' is full. In both cases, if we're not careful, our
     * calculated shift values will be equal to 64, which is UB. */
    bitsAddedToBuffer = 64 - stream->bits_in_buffer;
    bitsAddedToExtraBuffer = extraBits - bitsAddedToBuffer;
    assert(bitsAddedToExtraBuffer < 8);

    /* NOTE: When 'bitsAddedToBuffer is 64, we won't shift, but the mask will also be zero */
    extraBufferMaskLow = ((uint64_t)1 << bitsAddedToExtraBuffer) - 1;
    assert((bitsAddedToBuffer < 64) || (extraBufferMaskLow == 0));
    stream->extra_buffer = (uint8_t)(extraData >> (bitsAddedToBuffer & 0x3F)) & (uint8_t)extraBufferMaskLow;
    stream->bits_in_extra_buffer = bitsAddedToExtraBuffer;

    /* NOTE: This will make all bits higher than 'extraBits' 1, but that's okay since they should be zero */
    /* NOTE: Additionally, in the case 'bitsAddedToBuffer' is 64, we won't shift at all, but that's also okay since
     *       'extraBuffermaskLow' should be zero in that case */
    bufferMask = ~(extraBufferMaskLow << (bitsAddedToBuffer & 0x3F));
    stream->buffer |= (extraData & bufferMask) << (stream->bits_in_buffer & 0x3F);
    stream->bits_in_buffer = 64;

    return 64;
}

static int bitstream_fill_buffer_small(bitstream* stream)
{
    /* This is mostly the same as 'bitstream_fill_buffer', however we know that we don't have enough input to fully fill
     * 'buffer'. This means that 'extra_buffer' will be empty at the end of this function, allowing us to make some
     * optimizations */
    uint64_t extraData = stream->extra_buffer;
    int extraBits = stream->bits_in_extra_buffer;
    assert(stream->length < 8); /* Otherwise we could fill the buffer */
    assert((stream->bits_in_buffer + stream->bits_in_extra_buffer + stream->length * 8) < 64); /* Sanity check */

    /* TODO: Test doing this in reverse; allows us to skip incrementing both data and extraBits in the switch */
    switch (stream->length)
    {
    case 7:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 6:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 5:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 4:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 3:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 2:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 1:
        extraData |= ((uint64_t)*stream->data) << extraBits;
        ++stream->data;
        extraBits += 8;
        /* Fallthrough */
    case 0:
        break;
    default:
        /* TODO: Unreachable */
        assert(0); /* Should never happen */
        break;
    }

    stream->buffer |= extraData << stream->bits_in_buffer;
    stream->bits_in_buffer += extraBits;
    assert(stream->bits_in_buffer < 64); /* Yet another sanity check */

    stream->length = 0; /* We'll always consume the rest of the input */

    stream->extra_buffer = 0;
    stream->bits_in_extra_buffer = 0;

    return stream->bits_in_buffer;;
}

int bitstream_read_bits(bitstream* stream, int bitsToRead, uint16_t* result)
{
    uint32_t mask;

    /* The caller is expected to fill the buffer prior to calling this function */
    assert((stream->bits_in_buffer >= bitsToRead) || ((stream->bits_in_extra_buffer == 0) && (stream->length == 0)));

    if (stream->bits_in_buffer < bitsToRead)
    {
        return 0; /* Not enough data */
    }

    *result = bitstream_read_bits_unchecked(stream, bitsToRead);

    return 1;
}

uint16_t bitstream_read_bits_unchecked(bitstream* stream, int bitsToRead)
{
    uint64_t mask;
    uint16_t result;

    assert((bitsToRead > 0) && (bitsToRead <= (sizeof(result) * 8)));
    assert(stream->bits_in_buffer >= bitsToRead);

    mask = ((uint64_t)1 << bitsToRead) - 1;
    result = stream->buffer & mask;
    stream->buffer >>= bitsToRead;
    stream->bits_in_buffer -= bitsToRead;
    /* NOTE: We don't touch extra_buffer; that'll get used on the next fill call s*/

    return result;
}

int bitstream_peek(bitstream* stream, uint16_t* result)
{
    /* NOTE: It's valid to call this function even when there is still data in the input and fewer than 16 bits in the
     * buffer, e.g. in scenarios where we expect to consume a maximum of N < 16 and we've already ensured the buffer has
     * at least N bits (e.g. by a prior call to bitstream_fill_buffer) */
    *result = (uint16_t)stream->buffer;
    return (stream->bits_in_buffer <= 16) ? stream->bits_in_buffer : 16;
}

void bitstream_consume_bits(bitstream* stream, int bits)
{
    assert(bits <= stream->bits_in_buffer);

    stream->buffer >>= bits;
    stream->bits_in_buffer -= bits;
}
