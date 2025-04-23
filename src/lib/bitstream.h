
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
        // Read buffer
        const uint8_t* data;
        size_t length;

        // Partially read data
        uint32_t buffer;
        int bits_in_buffer;
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
     * Reads the specified number of bits, writing to 'result'. This function returns 1 if all bits could be read and 0
     * if more data is needed. In the failure case, the contents of 'result' are unspecified and no data is consumed
     */
    int bitstream_read_bits(bitstream* stream, int bitsToRead, uint16_t* result);

    /*
     * Peeks whatever data is available, returning the number of bits in the result. The data is NOT consumed
     */
    int bitstream_peek(bitstream* stream, uint16_t* result);

    /*
     * Consumes the specified number of bits, 1-16, from the input buffer and disposes of them. The caller is
     * responsible for ensuring that the buffer has at least the specified number of bits (e.g. by first calling
     * bitstream_peek).
     */
    void bitstream_consume_bits(bitstream* stream, int bits);

    /* Same as the above functions, but does not check to verify that the bitstream has enough input data */
    uint16_t bitstream_read_bits_unchecked(bitstream* stream, int bitsToRead);
    uint16_t bitstream_peek_unchecked(bitstream* stream);

#ifdef __cplusplus
}
#endif

#endif
