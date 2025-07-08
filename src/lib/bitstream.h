
#ifndef INFLATELIB_BITSTREAM_H
#define INFLATELIB_BITSTREAM_H

#include <stdint.h>

#ifdef __cplusplus
// Needed for the tests
extern "C"
{
#endif

    typedef struct bitstream
    {
        /* Read buffer */
        const uint8_t* data;
        size_t length;

        /* Partially read data */
        /* NOTE: This can hold more than a byte in situations where not enough data is available for a specific
         * operation, in which case its used as storage until the caller supplies more data */
        uint16_t partial_data;
        size_t partial_data_size;
    } bitstream;

    void bitstream_init(bitstream* stream);
    void bitstream_reset(bitstream* stream);
    void bitstream_set_data(bitstream* stream, const uint8_t* data, size_t length);
    const uint8_t* bitstream_clear_data(bitstream* stream, size_t* length);

    void bitstream_byte_align(bitstream* stream);

    /*
     * Copies at most the specified number of bytes to 'dest', returning the number of bytes that were actually copied.
     * The data in the stream must be byte aligned, otherwise the behavior is undefined.
     */
    size_t bitstream_copy_bytes(bitstream* stream, size_t bytesToRead, uint8_t* dest);

    /*
     * Reads the specified number of bits, writing to 'result'. This function returns 1 if all bits could be read and 0
     * if more data is needed. In the failure case, the contents of 'result' are unspecified and no data is consumed
     */
    size_t bitstream_read_bits(bitstream* stream, size_t bitsToRead, uint16_t* result);

    /*
     * Peeks whatever data is available, returning the number of bits in the result. The data is NOT consumed
     */
    size_t bitstream_peek(bitstream* stream, uint16_t* result);

    /*
     * Called when we've determined that the remaining input buffer is not large enough for the next operation. This
     * function ensures that we always signal to the caller that we need more data via an empty buffer.
     */
    void bitstream_cache_input(bitstream* stream);

    /*
     * Consumes the specified number of bits from the input buffer and disposes of them. The caller is responsible for
     * ensuring that the buffer has at least the specified number of bits (e.g. by first calling bitstream_peek).
     */
    static inline void bitstream_consume_bits(bitstream* stream, size_t bits)
    {
        if (bits <= stream->partial_data_size)
        {
            stream->partial_data_size -= bits;
            stream->partial_data >>= bits;
        }
        else
        {
            size_t bytesToConsume, partialBits;

            bits -= stream->partial_data_size;
            bytesToConsume = bits / 8; /* Full bytes to consume; does not include any partial byte */
            partialBits = bits % 8; /* Bits to consume from the last byte */

            if (partialBits)
            {
                /* We need a partial read */
                assert(stream->length >= (bytesToConsume + 1));
                stream->partial_data = stream->data[bytesToConsume] >> partialBits;
                stream->partial_data_size = 8 - partialBits;

                stream->data += bytesToConsume + 1;
                stream->length -= (bytesToConsume + 1);
            }
            else
            {
                assert(stream->length >= bytesToConsume);
                stream->partial_data = 0;
                stream->partial_data_size = 0;

                stream->data += bytesToConsume;
                stream->length -= bytesToConsume;
            }
        }
    }

    /* Same as the above functions, but does not check to verify that the bitstream has enough input data */
    uint16_t bitstream_read_bits_unchecked(bitstream* stream, size_t bitsToRead);
    uint16_t bitstream_peek_unchecked(bitstream* stream);

#ifdef __cplusplus
}
#endif

#endif
