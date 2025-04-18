
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
        /* Input buffer */
        const uint8_t* data;
        size_t length;

        /* Partially read data */
        /* NOTE: The size of 'buffer' is optimized to allow a single length/distance pair to be read & processed without
         * needing to constantly check if there's enough data in the input.
         * For Deflate, this requires 15 + 5 + 15 + 13 = 48 bits in the buffer
         * For Deflate64, this requires 15 + 16 + 15 + 14 = 60 bits in the buffer */
        uint64_t buffer;
        int bits_in_buffer;

        /* The buffer may not be empty when we try and fill it, meaning that it's possible to have anywhere between 57
         * and 64 bits in the buffer if we only fill with whole bytes. I.e. it's possible for the buffer to have less
         * data than is necessary for a full Deflate64 operation. To work around this, we hold an extra (partial) byte
         * so that we can ensure the buffer is as full as possible */
        uint8_t extra_buffer;
        int bits_in_extra_buffer;
    } bitstream;

    void bitstream_init(bitstream* stream);
    void bitstream_reset(bitstream* stream);
    void bitstream_set_data(bitstream* stream, const uint8_t* data, size_t length);

    void bitstream_byte_align(bitstream* stream);

    /*
     * Copies at most the specified number of bytes to 'dest', returning the number of bytes that were actually copied.
     * The data in the stream must be byte aligned, otherwise the behavior is undefined.
     */
    int bitstream_copy_bytes(bitstream* stream, int bytesToRead, uint8_t* dest);

    /*
     * Attempts to fill the internal buffer with 64 bits of input data. The number of bits available in the internal
     * buffer is returned.
     */
    int bitstream_fill_buffer(bitstream* stream);

    /*
     * Reads the specified number of bits, writing to 'result'. This function returns 1 if all bits could be read and 0
     * if more data is needed. In the failure case, the contents of 'result' are unspecified and no data is consumed.
     *
     * NOTE: 'bitstream_fill_buffer' must be called to fill the buffer prior to calling this function
     */
    int bitstream_read_bits(bitstream* stream, int bitsToRead, uint16_t* result);

    /*
     * Consumes the specified number of bits from the input without first checking to ensure that enough data is
     * available. It is the caller's responsibility to ensure that enough data is available in the input (e.g. by using
     * the result of the call to bitstream_fill_buffer).
     *
     * NOTE: 'bitstream_fill_buffer' must be called to fill the buffer prior to calling this function
     */
    uint16_t bitstream_read_bits_unchecked(bitstream* stream, int bitsToRead);

    /*
     * Peeks whatever data is available, returning the number of bits in the result. The data is NOT consumed
     *
     * NOTE: 'bitstream_fill_buffer' must be called to fill the buffer prior to calling this function
     */
    int bitstream_peek(bitstream* stream, uint16_t* result);

    /*
     * Consumes the specified number of bits from the input buffer and disposes of them. The caller is responsible for
     * ensuring that at least 'bits' bits are available in the input buffer prior to calling this function.
     */
    void bitstream_consume_bits(bitstream* stream, int bits);

#ifdef __cplusplus
}
#endif

#endif
