
#include <catch.hpp>
#include <span>

#include <window.h>

static const auto inputData = []() {
    std::array<std::uint8_t, DEFLATE64_WINDOW_SIZE * 2> result; // 2x the size for when we wrap around

    // Generate an input sequence that doesn't have duplicate sequence(s) to ensure we're never reading stale data. The
    // simplest thing to do is to write 16-bit values in little-endian (endianness doesn't really matter)
    for (std::uint32_t i = 0; i < DEFLATE64_WINDOW_SIZE; ++i)
    {
        // NOTE: We add 1 so that the first two entries are not both zero
        result[i * 2] = static_cast<std::uint8_t>(i & 0xFF);
        result[i * 2 + 1] = static_cast<std::uint8_t>(i >> 8);
    }

    return result;
}();

static const std::span<const std::uint8_t, DEFLATE64_WINDOW_SIZE> firstHalf(inputData.data(), DEFLATE64_WINDOW_SIZE);
static const std::span<const std::uint8_t, DEFLATE64_WINDOW_SIZE> secondHalf(inputData.data() + DEFLATE64_WINDOW_SIZE, DEFLATE64_WINDOW_SIZE);
static const std::span<const std::uint8_t, DEFLATE64_WINDOW_SIZE> middleHalf(inputData.data() + DEFLATE64_WINDOW_SIZE / 2, DEFLATE64_WINDOW_SIZE);
static const std::span<const std::uint8_t, DEFLATE64_WINDOW_SIZE / 2> firstQuarter(inputData.data(), DEFLATE64_WINDOW_SIZE / 2);

static void read_data(window* window, std::span<std::uint8_t> output, std::span<const std::uint8_t> expectedData, std::size_t stride = DEFLATE64_WINDOW_SIZE)
{
    assert(output.size() >= expectedData.size());
    for (std::size_t bytesCopied = 0; bytesCopied < expectedData.size();)
    {
        auto bytesToCopy = std::min(expectedData.size() - bytesCopied, stride);
        auto bytesCopiedBack = window_copy_output(window, output.data() + bytesCopied, bytesToCopy);
        REQUIRE(bytesCopiedBack == bytesToCopy); // It's assumed we've already copied 'count' bytes

        bytesCopied += bytesToCopy;
    }

    REQUIRE(std::memcmp(expectedData.data(), output.data(), expectedData.size()) == 0);
}

TEST_CASE("WindowWriteBytesTest", "[window]")
{
    std::uint8_t out[DEFLATE64_WINDOW_SIZE];

    window window;
    window_init(&window);

    auto writeData = [&](std::span<const std::uint8_t> data, std::size_t stride) {
        bitstream stream;
        bitstream_init(&stream);
        for (std::size_t bytesCopied = 0; bytesCopied < data.size();)
        {
            auto bytesToCopy = std::min(data.size() - bytesCopied, stride);
            bitstream_set_data(&stream, data.data() + bytesCopied, bytesToCopy);

            // NOTE: The interface is that we pass the total number of bytes that we want, hence the subtraction
            auto actuallyCopied = window_copy_bytes(&window, &stream, static_cast<std::uint16_t>(bytesToCopy));
            REQUIRE(actuallyCopied == bytesToCopy); // Since we know exactly how big the buffer we set is

            bytesCopied += bytesToCopy;
        }
    };

    SECTION("Single byte test")
    {
        // Write and read all the bytes one at a time in lockstep. This will wrap around the window once
        for (std::size_t i = 0; i < inputData.size(); ++i)
        {
            std::span<const std::uint8_t> data(inputData.data() + i, 1);
            writeData(data, 1);
            read_data(&window, out, data, 1);
        }

        // Do the test again, but this time write all data first before reading. In this case, we'll have to limit
        // ourselves to just 1x the window size
        writeData(firstHalf, 1);
        read_data(&window, out, firstHalf, 1);
    }

    SECTION("Full buffer test")
    {
        // Start by writing all data. This is the most convenient time to ensure we're reading/writing from the
        // beginning to the end of the buffer
        // NOTE: Because NLEN is a 16-bit value (and therefore the argument to window_copy_bytes is 16-bits), we can't
        // fill the entire buffer in a single call, so we need to split it into at least 2 calls
        writeData(firstHalf, 0xFFFF);
        read_data(&window, out, firstHalf);

        // Now repeat, but do the writes in two equal steps
        writeData(secondHalf, 0x8000);
        read_data(&window, out, secondHalf);

        // Write half the data so that a later write/read will have to wrap to the start of the buffer
        writeData(firstQuarter, DEFLATE64_WINDOW_SIZE);
        read_data(&window, out, firstQuarter); // Need to consume

        writeData(secondHalf, 0xFFFF);
        read_data(&window, out, secondHalf);
    }

    SECTION("Different size read/writes")
    {
        // Write in 256 byte chunks, but read in 128 byte chunks. This will take 128 iterations before the writes wrap
        // around to the start of the window and there are '256 * N - 128 * (N - 1)' unread bytes in the window after N
        // writes. That means it'll take 511 writes (510 reads) before the buffer is full and another 512 reads to
        // consume all of the data
        for (std::size_t i = 0; i < 1022; ++i)
        {
            if (i < 511)
            {
                std::span<const std::uint8_t> dataToWrite(inputData.data() + (i % 256) * 256, 256);
                writeData(dataToWrite, dataToWrite.size());
            }

            std::span<const std::uint8_t> dataToRead(inputData.data() + (i % 512) * 128, 128);
            read_data(&window, out, dataToRead, dataToRead.size());
        }
    }

    REQUIRE(window.unconsumed_bytes == 0);
}

TEST_CASE("WindowWriteByteTest", "[window]")
{
    std::uint8_t out[DEFLATE64_WINDOW_SIZE];
    window window;
    window_init(&window);

    auto writeData = [&](std::span<const std::uint8_t> data) {
        for (auto byte : data)
        {
            REQUIRE(window_write_byte(&window, byte));
        }
    };

    // Fill the buffer by manually writing bytes
    writeData(firstHalf);
    read_data(&window, out, firstHalf);

    // Do it again with different data to ensure the window wraps around
    writeData(secondHalf);
    read_data(&window, out, secondHalf);

    // Since we're writing single bytes, it doesn't really benefit us much to test wrapping around the window more than
    // once, however it's at least somewhat interesting to do it with unread data
    writeData(firstQuarter);
    read_data(&window, out, firstQuarter); // Need to read/write _something_ so we're not starting at the beginning
    writeData(middleHalf);
    read_data(&window, out, middleHalf);

    REQUIRE(window.unconsumed_bytes == 0);
}

TEST_CASE("WindowWrite", "[window]")
{
    std::uint8_t output[DEFLATE64_WINDOW_SIZE];

    window window;
    window_init(&window);

    bitstream stream;
    bitstream_init(&stream);
    bitstream_set_data(&stream, firstHalf.data(), firstHalf.size());

    auto writeSomeBytes = [&](std::uint16_t count) {
        REQUIRE(window_copy_bytes(&window, &stream, count) == count);

        // Read it back since the purpose of this function is just to fill the buffer with data for us to reference
        REQUIRE(window_copy_output(&window, output, std::size(output)) == count);
    };

    SECTION("Error cases")
    {
        // All of these use a distance larger than the number of bytes that have been written
        REQUIRE(window_copy_length_distance(&window, 1, 1) == -1);     // No data written yet
        REQUIRE(window_copy_length_distance(&window, 65536, 1) == -1); // Overflow shouldn't interpret this as zero

        // Write some data so it's not empty, but the distances still exceed this amount
        REQUIRE(window_copy_bytes(&window, &stream, 256) == 256);
        REQUIRE(window_copy_length_distance(&window, 257, 1) == -1);

        REQUIRE(window_copy_bytes(&window, &stream, 65279) == 65279); // Fill up buffer minus one byte
        REQUIRE(window_copy_length_distance(&window, 65536, 1) == -1);
    }
    SECTION("Non-overlapping")
    {
        // Write some data into the window so we can reference it
        writeSomeBytes(256);

        auto writeData = [&](std::uint32_t iteration) {
            auto repetitions = 1 << iteration;
            std::uint32_t len = 256 << iteration;

            REQUIRE(window_copy_length_distance(&window, len, len) == len);
            REQUIRE(window_copy_output(&window, output, std::size(output)) == len);
            for (uint32_t i = 0; i < repetitions; ++i)
            {
                REQUIRE(std::memcmp(firstHalf.data(), output + 256 * i, 256) == 0);
            }
        };

        // Continuously using window_copy_length_distance to duplicate what we previously wrote will require 9
        // iterations to get to the point where the duplication is the window size
        for (std::uint32_t i = 0; i < 9; ++i)
        {
            writeData(i);
        }
    }
    SECTION("Overlapping repetitions")
    {
        // Write some data into the window and then use the function to repeat it a bunch
        writeSomeBytes(256);

        // Repeat it twice
        REQUIRE(window_copy_length_distance(&window, 256, 512) == 512);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 512);
        REQUIRE(std::memcmp(firstHalf.data(), output, 256) == 0);
        REQUIRE(std::memcmp(firstHalf.data(), output + 256, 256) == 0);

        // Now repeat it 256 times
        REQUIRE(window_copy_length_distance(&window, 256, 65536) == 65536);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 65536);
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            REQUIRE(std::memcmp(firstHalf.data(), output + 256 * i, 256) == 0);
        }
    }
    SECTION("Maximums")
    {
        // Fill the buffer; this must be done in two steps since the max write size is one less than the buffer
        writeSomeBytes(0x8000);
        writeSomeBytes(0x8000);

        // The maximum distance is 65536 and the max length is 65538. This is larger than the window size, so the first
        // call can't write all of the data until we read some of it back
        REQUIRE(window_copy_length_distance(&window, 65536, 65538) == 65536);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 65536);
        REQUIRE(std::memcmp(firstHalf.data(), output, DEFLATE64_WINDOW_SIZE) == 0);
        REQUIRE(window_copy_length_distance(&window, 65536, 2) == 2); // Simulate copying the rest of the data
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 2);
        REQUIRE(std::memcmp(firstHalf.data(), output, 2) == 0); // This wraps back around

        // Now do the same thing again, but this time start in the middle of the buffer
        bitstream_set_data(&stream, firstQuarter.data(), firstQuarter.size());
        writeSomeBytes(0x8000);
        bitstream_set_data(&stream, secondHalf.data(), secondHalf.size());
        writeSomeBytes(0x8000);
        writeSomeBytes(0x8000);

        // The operations are the exact same as before
        REQUIRE(window_copy_length_distance(&window, 65536, 65538) == 65536);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 65536);
        REQUIRE(std::memcmp(secondHalf.data(), output, DEFLATE64_WINDOW_SIZE) == 0);
        REQUIRE(window_copy_length_distance(&window, 65536, 2) == 2); // Simulate copying the rest of the data
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 2);
        REQUIRE(std::memcmp(secondHalf.data(), output, 2) == 0); // This wraps back around
    }
    SECTION("Curated Conditions")
    {
        // Test very specific conditions around overlap and overflow (wrapping around to start of the buffer). For all
        // tests, we split the buffer into four quarters and then shift the write index by an eighth of the buffer so
        // that when we test overflow, and specifically no overflow scenarios, we either go past the end of the buffer
        // or we don't even approach it
        static constexpr const std::uint32_t one_half = 0x8000;
        static constexpr const std::uint32_t one_quarter = 0x4000;
        static constexpr const std::uint32_t one_eighth = 0x2000;
        writeSomeBytes(one_eighth * 5);

        // Copy from earlier in the buffer, no overlap, no overflow
        // Write pointer is at 5/8; copy 25% of the window starting at the beginning of the window (minus 5/8)
        REQUIRE(window_copy_length_distance(&window, 5 * one_eighth, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data(), output, one_quarter) == 0);

        // Copy from earlier in the buffer, no overlap, overflow write
        // Write pointer is at 7/8; copy 25% of the window starting at 1/4 from the beginning of the window (minus 5/8)
        REQUIRE(window_copy_length_distance(&window, 5 * one_eighth, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output, one_quarter) == 0);

        // Copy from later in the buffer, no overlap, no overflow
        // Write pointer is at 1/8; copy 25% of the window starting at 5/8 from the beginning of the window (minus 1/2)
        REQUIRE(window_copy_length_distance(&window, one_half, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data(), output, one_quarter) == 0);

        // Copy from later in the buffer, no overlap, overflow read
        // Write pointer is at 3/8; copy 25% of the window starting at 7/8 from the beginning of the window (minus 1/2)
        REQUIRE(window_copy_length_distance(&window, one_half, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output, one_quarter) == 0);

        // Copy from later in the buffer, overlap, overflow read and write
        // Write pointer is at 5/8; copy 50% of the window starting at 7/8 from the beginning of the window (minus 3/4)
        REQUIRE(window_copy_length_distance(&window, 3 * one_quarter, one_half) == one_half);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_half);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output, one_quarter) == 0);
        REQUIRE(std::memcmp(firstHalf.data(), output + one_quarter, one_quarter) == 0);

        // Copy from earlier in the buffer, overlap, overflow read
        // Write pointer is at 1/8; copy 50% of the window starting at 7/8 from the beginning of the window (minus 1/4)
        REQUIRE(window_copy_length_distance(&window, one_quarter, one_half) == one_half);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_half);
        REQUIRE(std::memcmp(firstHalf.data(), output, one_quarter) == 0);
        REQUIRE(std::memcmp(firstHalf.data(), output + one_quarter, one_quarter) == 0);

        // Quickly reset the buffer... if we continue from here we'll just have the first quarter of 'firstHalf' to
        // continuously read over and over again... Note that we've written 5/8 of the buffer so before we go any
        // further, we need to finish writing the rest
        writeSomeBytes(one_eighth * 3);
        bitstream_set_data(&stream, firstHalf.data(), firstHalf.size());
        writeSomeBytes(one_eighth * 5);

        // Copy from earlier in the buffer, overlap, no overflow
        // Write pointer is at 5/8; copy 25% of the window starting at 1/2 from the beginning of the window (minus 1/8)
        REQUIRE(window_copy_length_distance(&window, one_eighth, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data() + one_half, output, one_eighth) == 0);
        REQUIRE(std::memcmp(firstHalf.data() + one_half, output + one_eighth, one_eighth) == 0);

        // Copy from later in the buffer, overlap, overflow write
        // Writer pointer is at 7/8; copy 25% of the window starting at the beginning of the window (minus 7/8)
        REQUIRE(window_copy_length_distance(&window, 7 * one_eighth, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data(), output, one_quarter) == 0);

        // Copy from later in the buffer, overlap, no overflow
        // Writer pointer is at 1/8; copy 25% of the window starting at 1/4 from the beginning of the window (minus 7/8)
        REQUIRE(window_copy_length_distance(&window, 7 * one_eighth, one_quarter) == one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_quarter);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output, one_quarter) == 0);

        // Copy from later in the buffer, overlap, overflow read
        // Write pointer is at 3/8; Copy 50% of the window starting at 3/4 from the beginning of the window (minus 5/8)
        REQUIRE(window_copy_length_distance(&window, 5 * one_eighth, one_half) == one_half);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_half);
        REQUIRE(std::memcmp(firstHalf.data() + one_half, output, one_eighth) == 0);
        REQUIRE(std::memcmp(firstHalf.data(), output + one_eighth, 3 * one_eighth) == 0);

        // Copy from earlier in the buffer, overlap, overflow read and write
        // Write pointer is at 7/8; copy 50% of the window starting at 5/8 from the beginning of the window (minus 1/4)
        REQUIRE(window_copy_length_distance(&window, one_quarter, one_half) == one_half);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == one_half);
        REQUIRE(std::memcmp(firstHalf.data() + one_eighth, output, one_quarter) == 0);
        REQUIRE(std::memcmp(firstHalf.data() + one_eighth, output + one_quarter, one_quarter) == 0);

        // Copy from earlier in the buffer, overlap, overflow write
        // Write pointer is at 3/8; copy 75% of the window starting at the beginning of the window (minus 3/8)
        REQUIRE(window_copy_length_distance(&window, 3 * one_eighth, 3 * one_quarter) == 3 * one_quarter);
        REQUIRE(window_copy_output(&window, output, std::size(output)) == 3 * one_quarter);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output, one_eighth) == 0);
        REQUIRE(std::memcmp(firstHalf.data() + one_eighth, output + one_eighth, one_quarter) == 0);
        REQUIRE(std::memcmp(firstHalf.data() + one_quarter, output + 3 * one_eighth, one_eighth) == 0);
        REQUIRE(std::memcmp(firstHalf.data() + one_eighth, output + one_half, one_quarter) == 0);
    }
}
