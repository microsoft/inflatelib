
#include <catch.hpp>
#include <span>

#include <bitstream.h>

static void DoBitstreamReadBitsTest(std::uint16_t value)
{
    // Create two bytes arrays: one where the value is byte aligned, and another where it is offset by 5 bits
    // (arbitrarily chosen prime value)
    const std::uint8_t aligned[] = {(std::uint8_t)value, (std::uint8_t)(value >> 8)};
    const std::uint8_t unaligned[] = {(std::uint8_t)(0x15 | (value << 5)), (std::uint8_t)(value >> 3), (std::uint8_t)(value >> 11)};

    int deadBits[] = {0, 5}; // Number of bits to "throw away"
    std::span<const std::uint8_t> data[] = {aligned, unaligned};
    for (std::size_t i = 0; i < std::size(data); ++i)
    {
        // Read all possible number of bits
        for (int bits = 1; bits <= 16; ++bits)
        {
            bitstream stream;
            bitstream_init(&stream);
            bitstream_set_data(&stream, data[i].data(), data[i].size());
            REQUIRE(bitstream_fill_buffer(&stream) == data[i].size() * 8);

            // Read any dead bits, if necessary
            std::uint16_t output;
            if (deadBits[i] != 0)
            {
                REQUIRE(bitstream_read_bits(&stream, deadBits[i], &output));
            }

            REQUIRE(bitstream_read_bits(&stream, bits, &output));

            auto expectedValue = value & static_cast<std::uint16_t>((1u << bits) - 1);
            REQUIRE(output == expectedValue);
        }
    }
}

TEST_CASE("BitstreamReadBits", "[bitstream]")
{
    // We can test all possible values in virtually no time, so just do it
    for (int i = 0; i <= 0xFFFF; ++i)
    {
        DoBitstreamReadBitsTest(static_cast<uint16_t>(i));
    }

    // Out of data reads
    SECTION("Out of Data")
    {
        const std::uint8_t data[] = {0xAB, 0xCD, 0x53};

        bitstream stream;
        bitstream_init(&stream);
        bitstream_set_data(&stream, data, 2);
        REQUIRE(bitstream_fill_buffer(&stream) == 16);

        // Read a few bits so that some of our future reads fail
        std::uint16_t value;
        REQUIRE(bitstream_read_bits(&stream, 4, &value));
        REQUIRE(value == 0x0B); // Read lowest bits first

        REQUIRE(!bitstream_read_bits(&stream, 16, &value));
        REQUIRE(stream.length == 0); // Should have consumed all data
        REQUIRE(stream.bits_in_buffer == 12);
        REQUIRE(!bitstream_read_bits(&stream, 15, &value)); // Should still fail
        REQUIRE(!bitstream_read_bits(&stream, 14, &value));
        REQUIRE(!bitstream_read_bits(&stream, 13, &value));

        REQUIRE(bitstream_read_bits(&stream, 12, &value)); // Finally success
        REQUIRE(value == 0x0CDA);

        // All data consumed with the last read
        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 0);

        // Reset the buffer, but add one byte at a time until we have enough data
        bitstream_set_data(&stream, data, 1);
        REQUIRE(bitstream_fill_buffer(&stream) == 8);
        REQUIRE(bitstream_read_bits(&stream, 4, &value)); // Partial read, just to test more paths
        REQUIRE(value == 0x0B);

        REQUIRE(!bitstream_read_bits(&stream, 16, &value));
        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 4);

        bitstream_set_data(&stream, data + 1, 1);
        REQUIRE(bitstream_fill_buffer(&stream) == 12); // 4 leftover bits from before
        REQUIRE(!bitstream_read_bits(&stream, 16, &value));
        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 12);

        bitstream_set_data(&stream, data + 2, 1);
        REQUIRE(bitstream_fill_buffer(&stream) == 20); // 12 leftover bits from before
        REQUIRE(bitstream_read_bits(&stream, 16, &value)); // Finally success
        REQUIRE(value == 0x3CDA);
        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 4);
    }

    // Reading two 16-bit values back-to-back after a partial read should succeed (i.e. buffer is large enough)
    SECTION("Buffer size")
    {
        // 7-bit read followed by two 16-bit reads. Should give the values [ 0x7F, 0xAA55, 0xC639]
        const std::uint8_t data[] = {0xFF, 0x2A, 0xD5, 0x1C, 0x63};

        bitstream stream;
        bitstream_init(&stream);
        bitstream_set_data(&stream, data, std::size(data));
        REQUIRE(bitstream_fill_buffer(&stream) == 8 * std::size(data));

        std::uint16_t value;
        REQUIRE(bitstream_read_bits(&stream, 7, &value));
        REQUIRE(value == 0x7F);

        REQUIRE(bitstream_read_bits(&stream, 16, &value));
        REQUIRE(value == 0xAA55);

        REQUIRE(bitstream_read_bits(&stream, 16, &value));
        REQUIRE(value == 0xC639);

        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 1);
    }
}

TEST_CASE("BitstreamCopyBytes", "[bitstream]")
{
    // 2 bytes of "dead" input that we'll read before reading the rest of the data (16 more bytes)
    static constexpr const std::uint8_t input[] = {
        0xC3, 0xA5, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    for (int readCount = 1; readCount <= 16; ++readCount)
    {
        auto doTest = [&](bitstream& stream, int bytesToRead, int expectedRead) {
            std::uint16_t ignore;
            REQUIRE(bitstream_fill_buffer(&stream) >= 16); // Somewhat unpredictable... should be at least enough for our two reads
            REQUIRE(bitstream_read_bits(&stream, readCount, &ignore));
            bitstream_byte_align(&stream); // Skip to next byte
            if (readCount <= 8)
            {
                // Need to read another byte, since we have 2 dead bytes
                REQUIRE(bitstream_read_bits(&stream, readCount, &ignore));
                bitstream_byte_align(&stream);
            }

            std::uint8_t buffer[16];
            REQUIRE(bitstream_copy_bytes(&stream, bytesToRead, buffer) == expectedRead);
            REQUIRE(::memcmp(buffer, input + 2, expectedRead) == 0);
        };

        // Test 1: Set to full size, but read a subset of bytes
        bitstream stream;
        bitstream_init(&stream);
        bitstream_set_data(&stream, input, std::size(input));
        doTest(stream, readCount, readCount);

        // Test 2: Set to smaller size & attempt to read more bytes
        bitstream_reset(&stream);
        bitstream_set_data(&stream, input, 2 + readCount);
        doTest(stream, 16, readCount);
    }
}

TEST_CASE("BitstreamPeekConsume", "[bitstream]")
{
    bitstream stream;
    bitstream_init(&stream);

    std::array<std::uint8_t, 10> buffer; // Just a buffer we can easily write data to for input
    std::uint16_t value;                 // Value we can write output to

    // Calling peek twice should give the same data
    buffer = {0x21, 0x84};
    bitstream_set_data(&stream, buffer.data(), 2);
    REQUIRE(bitstream_fill_buffer(&stream) == 16);
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0x8421);
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0x8421);

    // Available data should be reduced after consuming
    bitstream_consume_bits(&stream, 4);
    REQUIRE(bitstream_peek(&stream, &value) == 12);
    REQUIRE(value == 0x842);
    REQUIRE(bitstream_peek(&stream, &value) == 12);
    REQUIRE(value == 0x842);

    buffer = {0xAC};
    bitstream_set_data(&stream, buffer.data(), 1);
    REQUIRE(bitstream_fill_buffer(&stream) == 20);
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0xC842);
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0xC842);

    bitstream_consume_bits(&stream, 10);
    REQUIRE(bitstream_peek(&stream, &value) == 10);
    REQUIRE(value == 0x2B2);
    REQUIRE(bitstream_peek(&stream, &value) == 10);
    REQUIRE(value == 0x2B2);

    // Consume the rest
    bitstream_consume_bits(&stream, 10);
    REQUIRE(bitstream_peek(&stream, &value) == 0);
    REQUIRE(value == 0);
}

TEST_CASE("BitstreamFillBuffer", "[bitstream]")
{
    std::array<std::uint8_t, 16> input = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::array<std::uint8_t, 16> output;
    std::uint16_t value;

    auto checkReadBytes = [&](std::size_t offset, std::size_t count) {
        REQUIRE(std::memcmp(input.data() + offset, output.data(), count) == 0);
    };

    bitstream stream;
    bitstream_init(&stream);

    bitstream_set_data(&stream, input.data(), input.size());

    SECTION("Read from buffer")
    {
        // Filling the buffer when it's empty should only fill the buffer with 64 bits
        REQUIRE(bitstream_fill_buffer(&stream) == 64);
        REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
        REQUIRE(stream.extra_buffer == 0);
        REQUIRE(stream.bits_in_extra_buffer == 0);

        // Calling again should effectively no-op
        REQUIRE(bitstream_fill_buffer(&stream) == 64);
        REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
        REQUIRE(stream.extra_buffer == 0);
        REQUIRE(stream.bits_in_extra_buffer == 0);

        // Reading one byte and then filling it again should still only fill the buffer
        REQUIRE(bitstream_copy_bytes(&stream, 1, output.data()) == 1);
        checkReadBytes(0, 1);
        REQUIRE(stream.bits_in_buffer == 7 * 8); // 7 bytes left after reading one byte

        REQUIRE(bitstream_fill_buffer(&stream) == 64);
        REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
        REQUIRE(stream.extra_buffer == 0);
        REQUIRE(stream.bits_in_extra_buffer == 0);

        // Reading 8 bytes should exhaust the buffer
        REQUIRE(bitstream_copy_bytes(&stream, 8, output.data()) == 8);
        checkReadBytes(1, 8);
        REQUIRE(stream.bits_in_buffer == 0); 

        // There's now 7 bytes remaining in the input; filling should only fill those 7 bytes
        REQUIRE(bitstream_fill_buffer(&stream) == 7 * 8);
        REQUIRE(bitstream_fill_buffer(&stream) == 7 * 8); // Calling again okay/no-op
        REQUIRE(stream.extra_buffer == 0);
        REQUIRE(stream.bits_in_extra_buffer == 0);

        // Reading should consume all of the data from the buffer
        REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 7);
    }

    SECTION("Read past buffer")
    {
        SECTION("Full buffer")
        {
            REQUIRE(bitstream_fill_buffer(&stream) == 64);
            REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
            REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == input.size());
            checkReadBytes(0, input.size());
        }

        SECTION("Partial buffer")
        {
            REQUIRE(bitstream_fill_buffer(&stream) == 64);
            REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
            REQUIRE(bitstream_copy_bytes(&stream, 6, output.data()) == 6);
            checkReadBytes(0, 6);

            REQUIRE(stream.bits_in_buffer == 2 * 8); // 2 bytes left in the buffer
            REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 10);
            checkReadBytes(6, 10);
        }

        SECTION("Byte aligned buffer")
        {
            REQUIRE(bitstream_fill_buffer(&stream) == 64);
            REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
            REQUIRE(bitstream_read_bits(&stream, 5, &value));
            REQUIRE(value == 0x00);

            // This will read data into the extra buffer
            REQUIRE(bitstream_fill_buffer(&stream) == 64);
            REQUIRE(bitstream_fill_buffer(&stream) == 64); // Calling again okay/no-op
            REQUIRE(stream.bits_in_extra_buffer == 3);

            // This will move the data from the extra buffer into the main buffer
            bitstream_byte_align(&stream);
            REQUIRE(stream.bits_in_buffer == 64);
            REQUIRE(stream.bits_in_extra_buffer == 0);
            REQUIRE(stream.extra_buffer == 0);

            // We've already read one byte; this should read the rest
            REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 15);
            checkReadBytes(1, 15);
        }
    }

    SECTION("Single Bit Reads")
    {
        // Loop until we've read every bit, reading one bit at a time
        int bitsRemaining = 8 * input.size();
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            for (std::size_t bit = 0; bit < 8; ++bit)
            {
                INFO("Reading bit " << bit << " of byte " << i);
                auto expectedBits = std::min(64, bitsRemaining--);
                REQUIRE(bitstream_fill_buffer(&stream) == expectedBits);
                REQUIRE(bitstream_fill_buffer(&stream) == expectedBits); // Calling again okay/no-op

                REQUIRE(bitstream_read_bits(&stream, 1, &value));
                REQUIRE(value == ((input[i] >> bit) & 0x01));
            }
        }

        REQUIRE(stream.length == 0);
        REQUIRE(stream.bits_in_buffer == 0);
        REQUIRE(stream.bits_in_extra_buffer == 0);
    }

    SECTION("Multi Bit Reads")
    {
        // Read 5 bits at a time. There's a total of 128 bits in the input, so we should be able to read 25 times
        // without overflowing with a single 3 bit read at the end
        int bitsRemaining = 8 * input.size();
        std::size_t i = 0, bit = 0;
        while (bitsRemaining >= 5)
        {
            INFO("Starting at bit " << bit << " of byte " << i);
            auto expectedBits = std::min(64, bitsRemaining);
            REQUIRE(bitstream_fill_buffer(&stream) == expectedBits);
            REQUIRE(bitstream_fill_buffer(&stream) == expectedBits); // Calling again okay/no-op

            std::uint8_t expectedValue = input[i] >> bit;
            bit += 5;
            if (bit >= 8)
            {
                ++i;
                bit -= 8;
                expectedValue |= (uint8_t)((uint32_t)input[i] << (5 - bit));
            }

            expectedValue &= 0x1F; // Mask to 5 bits
            REQUIRE(bitstream_read_bits(&stream, 5, &value));
            REQUIRE(value == expectedValue);

            bitsRemaining -= 5;
        }
    }
}

TEST_CASE("BitstreamAlternatingReads", "[bitstream]")
{
    // Alternating calls to read/peek bits and read bytes should work as expected. Effectively this ensures that even in
    // scenarios where we fill up the internal buffer, we will still read those bytes if they go unused
    bitstream stream;
    bitstream_init(&stream);

    std::array<std::uint8_t, 10> input = {0xFF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xCC};
    std::array<std::uint8_t, 10> output; // A buffer we can read bytes into
    std::uint16_t value;                 // Value we can write output to

    auto checkReadBytes = [&](std::size_t offset, std::size_t count) {
        REQUIRE(std::memcmp(input.data() + offset, output.data(), count) == 0);
    };

    // First test: read less than a byte, align, then read a few bytes
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    REQUIRE(bitstream_read_bits(&stream, 6, &value));
    REQUIRE(value == 0x3F);
    bitstream_byte_align(&stream);
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 9);
    checkReadBytes(1, 9);

    // Second test: do a peek, which should only consume a few bits. When we then read the rest of the bytes, we should
    // first read from what's left in the buffer
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    REQUIRE(bitstream_peek(&stream, &value) == 16); // At least two bytes of data are available
    REQUIRE(value == 0x01FF);
    bitstream_consume_bits(&stream, 8);
    bitstream_byte_align(&stream); // Required before calling bitstream_copy_bytes
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 9);
    checkReadBytes(1, 9);

    // Third test: do the same as the above, only this time consume a few bits & byte align
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    REQUIRE(bitstream_peek(&stream, &value) == 16); // At least two bytes of data are available
    REQUIRE(value == 0x01FF);
    bitstream_consume_bits(&stream, 6);
    bitstream_byte_align(&stream);
    bitstream_byte_align(&stream); // The additional align should no-op
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 9);
    checkReadBytes(1, 9);

    // Fourth test: same as the previous two, but read a couple bits before filling the buffer again to ensure that we
    // use the extra buffer
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    REQUIRE(bitstream_read_bits(&stream, 4, &value));
    REQUIRE(value == 0x0F);
    REQUIRE(bitstream_fill_buffer(&stream) == 64); // Should now be using the extra buffer
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0x301F);
    bitstream_byte_align(&stream);
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 9);
    checkReadBytes(1, 9);

    // Fifth test: similar to the above, but use 'bitstream_copy_bytes' to consume a byte from the buffer & ensure that
    // there's still data in the buffer
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    REQUIRE(bitstream_copy_bytes(&stream, 1, output.data()) == 1);
    REQUIRE(output[0] == 0xFF);
    REQUIRE(bitstream_read_bits(&stream, 16, &value));
    REQUIRE(value == 0x2301);
    bitstream_byte_align(&stream); // Required before calling bitstream_copy_bytes
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 7);
    checkReadBytes(3, 7);

    // Final test: Use all three operations to consume data from the buffer
    bitstream_set_data(&stream, input.data(), input.size());
    REQUIRE(bitstream_fill_buffer(&stream) == 64);
    bitstream_consume_bits(&stream, 1); // 63 bits left in the buffer
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0x80FF);
    bitstream_byte_align(&stream); // 56 bits left in the buffer
    REQUIRE(bitstream_copy_bytes(&stream, 1, output.data()) == 1); // 48 bits left in the buffer
    REQUIRE(output[0] == 0x01);
    REQUIRE(bitstream_peek(&stream, &value) == 16);
    REQUIRE(value == 0x4523);
    bitstream_consume_bits(&stream, 3); // 45 bits left in the buffer
    REQUIRE(bitstream_read_bits(&stream, 13, &value)); // 32 bits left in the buffer (byte aligned)
    REQUIRE(value == 0x08A4);
    bitstream_byte_align(&stream); // Required before calling bitstream_copy_bytes
    REQUIRE(bitstream_copy_bytes(&stream, output.size(), output.data()) == 6);
    checkReadBytes(4, 6);
}
