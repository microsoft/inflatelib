
#include <catch.hpp>

#include <inflatelib.hpp>
#include <filesystem>
#include <stdio.h>

// These tests have backing test files compiled from 'test/data' and placed into '${buildRoot}/test/data'. When running
// this test, that path is '../data' relative to the test executable.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
static std::filesystem::path executable_directory()
{
    wchar_t buffer[MAX_PATH];
    auto len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (!len || len == MAX_PATH)
    {
        throw std::runtime_error("Failed to get executable path");
    }

    return std::filesystem::canonical(buffer).parent_path();
}
#else // Otherwise, assume Linux
static std::filesystem::path executable_directory()
{
    return std::filesystem::canonical("/proc/self/exe").parent_path();
}
#endif

static const std::filesystem::path data_directory = executable_directory().parent_path() / "data";

struct file_deleter
{
    void operator()(std::FILE* file) const
    {
        if (file)
        {
            std::fclose(file);
        }
    }
};
using unique_file = std::unique_ptr<std::FILE, file_deleter>;

struct file_contents
{
    std::unique_ptr<std::byte[]> buffer;
    std::size_t size;
};

static file_contents read_file(const std::filesystem::path& path)
{
    FILE* handle;
#ifdef _WIN32
    if (auto err = ::_wfopen_s(&handle, path.c_str(), L"rb"))
#else
    if (auto err = ::fopen_s(&handle, path.c_str(), "rb"))
#endif
    {
        throw std::system_error(err, std::generic_category(), std::format("Failed to open file {}", path.string()));
    }

    unique_file file(handle);

    if (::fseek(file.get(), 0, SEEK_END) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to seek to end of file");
    }

    file_contents result;
    result.size = ::ftell(file.get());
    if (result.size == -1)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to get file size");
    }

    if (::fseek(file.get(), 0, SEEK_SET) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to seek to start of file");
    }

    result.buffer = std::make_unique<std::byte[]>(result.size);
    for (std::size_t i = 0; i < result.size;)
    {
        auto read = ::fread(result.buffer.get() + i, 1, result.size - i, file.get());
        if (read == 0)
        {
            throw std::system_error(EIO, std::generic_category(), "Failed to read file");
        }

        i += read;
    }

    return result;
}

using try_inflate_t = int (inflatelib::stream::*)(std::span<const std::byte>&, std::span<std::byte>&) noexcept;
using inflate_t = bool (inflatelib::stream::*)(std::span<const std::byte>&, std::span<std::byte>&);

template <try_inflate_t inflateFunc>
static void inflate_test_worker(
    const file_contents& input, const file_contents& output, std::size_t readStride, std::size_t writeStride, const char* errFragment)
{
    // Output buffer size is zero if we expect an error, but we may never see that error if we don't have a buffer to
    // write output to
    auto outputBufferSize = output.size ? output.size : 0x10000;
    auto outputBuffer = std::make_unique<std::byte[]>(outputBufferSize);

    inflatelib::stream stream;

    // Set up the spans that we use for input/output
    std::span<const std::byte> inputSpan = {input.buffer.get(), std::min(readStride, input.size)};
    std::span<std::byte> outputSpan = {outputBuffer.get(), std::min(writeStride, outputBufferSize)};

    int result;
    std::size_t readOffset = 0, writeOffset = 0;
    while ((readOffset < input.size) || (writeOffset < outputBufferSize))
    {
        result = (stream.*inflateFunc)(inputSpan, outputSpan);
        if (result < 0)
        {
            break;
        }

        readOffset = inputSpan.data() - input.buffer.get();
        writeOffset = outputSpan.data() - outputBuffer.get();

        // NOTE: Because our buffers are the full size of the input/output, we don't need to update pointers, however
        // we still want to wait until we've consumed all of each buffer before changing the sizes
        if (inputSpan.empty())
        {
            inputSpan = {input.buffer.get() + readOffset, std::min(readStride, input.size - readOffset)};
        }
        if (outputSpan.empty())
        {
            outputSpan = {outputBuffer.get() + writeOffset, std::min(writeStride, outputBufferSize - writeOffset)};
        }

        if (result > INFLATELIB_OK)
        {
            // E.g. EOF - we *should* meet the criteria for terminating the loop; we'll check that below
            break;
        }
    }

    if (result < 0)
    {
        UNSCOPED_INFO("The error message is: " << stream.error_msg());
    }

    if (errFragment)
    {
        INFO("Expecting error message: " << errFragment);
        INFO("Actual error message: " << stream.error_msg());
        REQUIRE(result == INFLATELIB_ERROR_DATA);
        REQUIRE(std::strstr(stream.error_msg(), errFragment) != nullptr);
    }
    else
    {
        REQUIRE(result == INFLATELIB_EOF);
        REQUIRE(readOffset == input.size);   // We should have read all the data
        REQUIRE(writeOffset == output.size); // Should match the expected output size
        REQUIRE(std::memcmp(outputBuffer.get(), output.buffer.get(), output.size) == 0);

        // Calling again should return EOF
        REQUIRE((stream.*inflateFunc)(inputSpan, outputSpan) == INFLATELIB_EOF);
        REQUIRE(stream.get()->next_in == input.buffer.get() + input.size);
        REQUIRE(stream.get()->next_out == outputBuffer.get() + output.size);
    }
}

template <try_inflate_t inflateFunc>
static void do_inflate_test(const file_contents& input, const file_contents& output, const char* errFragment)
{
    auto minOutputStride = output.size ? output.size : 0x10000;

    // Give ourselves our best opportunity for success; use strides equal to the sizes of the buffers
    inflate_test_worker<inflateFunc>(input, output, input.size, minOutputStride, errFragment);

    // Now with a (likely) smaller stride, but still large enough to cause issues with buffer size
    inflate_test_worker<inflateFunc>(input, output, 64, minOutputStride, errFragment);
    inflate_test_worker<inflateFunc>(input, output, input.size, 64, errFragment);
    inflate_test_worker<inflateFunc>(input, output, 64, 64, errFragment);

    // Now with a much smaller stride, but still large enough to at least hold full symbols
    inflate_test_worker<inflateFunc>(input, output, 7, minOutputStride, errFragment);
    inflate_test_worker<inflateFunc>(input, output, input.size, 7, errFragment);
    inflate_test_worker<inflateFunc>(input, output, 7, 7, errFragment);

    // And finally, just one byte at a time, which should be the most likely to cause issues
    inflate_test_worker<inflateFunc>(input, output, 1, minOutputStride, errFragment);
    inflate_test_worker<inflateFunc>(input, output, input.size, 1, errFragment);
    inflate_test_worker<inflateFunc>(input, output, 1, 1, errFragment);
}

static void inflate_test(const char* inputFileName, const char* outputFileName)
{
    auto input = read_file(data_directory / inputFileName);
    auto output = read_file(data_directory / outputFileName);
    do_inflate_test<&inflatelib::stream::try_inflate>(input, output, nullptr);
}

static void inflate64_test(const char* inputFileName, const char* outputFileName)
{
    auto input = read_file(data_directory / inputFileName);
    auto output = read_file(data_directory / outputFileName);
    do_inflate_test<&inflatelib::stream::try_inflate64>(input, output, nullptr);
}

static void inflate_error_test(const char* inputFileName, const char* errFragment)
{
    auto input = read_file(data_directory / inputFileName);
    do_inflate_test<&inflatelib::stream::try_inflate>(input, {}, errFragment);
}

static void inflate64_error_test(const char* inputFileName, const char* errFragment)
{
    auto input = read_file(data_directory / inputFileName);
    do_inflate_test<&inflatelib::stream::try_inflate64>(input, {}, errFragment);
}

TEST_CASE("InflateErrors", "[inflate]")
{
    inflate_error_test("error.invalid-block-type.in.bin", "Unexpected block type '3'");

    // Error if we call 'inflatelib_inflate' before calling 'inflate64_init'
    inflatelib_stream stream = {};
    REQUIRE(inflatelib_inflate(&stream) == INFLATELIB_ERROR_ARG);
}

TEST_CASE("Inflate64Errors", "[inflate64]")
{
    inflate64_error_test("error.invalid-block-type.in.bin", "Unexpected block type '3'");

    // Error if we call 'inflatelib_inflate64' before calling 'inflate64_init'
    inflatelib_stream stream = {};
    REQUIRE(inflatelib_inflate64(&stream) == INFLATELIB_ERROR_ARG);
}

TEST_CASE("InflateUncompressed", "[inflate]")
{
    inflate_test("uncompressed.empty.in.bin", "uncompressed.empty.out.bin");
    inflate_test("uncompressed.single.in.bin", "uncompressed.single.out.bin");
    inflate_test("uncompressed.multiple.in.bin", "uncompressed.multiple.out.bin");

    // Error cases
    inflate_error_test(
        "uncompressed.error.nlen.in.bin", "Uncompressed block length (7FFF) does not match its encoded one's complement value (0000)");
}

TEST_CASE("Inflate64Uncompressed", "[inflate64]")
{
    inflate64_test("uncompressed.empty.in.bin", "uncompressed.empty.out.bin");
    inflate64_test("uncompressed.single.in.bin", "uncompressed.single.out.bin");
    inflate64_test("uncompressed.multiple.in.bin", "uncompressed.multiple.out.bin");

    // Error cases
    inflate64_error_test(
        "uncompressed.error.nlen.in.bin", "Uncompressed block length (7FFF) does not match its encoded one's complement value (0000)");
}

TEST_CASE("InflateCompressedDynamic", "[inflate]")
{
    inflate_test("dynamic.empty.in.bin", "dynamic.empty.out.bin");
    // inflate_test("dynamic.single.deflate.in.bin", "dynamic.single.deflate.out.bin");
    // inflate_test("dynamic.multiple.deflate.in.bin", "dynamic.multiple.deflate.out.bin");
    // inflate_test("dynamic.overlap.deflate.in.bin", "dynamic.overlap.deflate.out.bin");
    // inflate_test("dynamic.length-distance-stress.deflate.in.bin", "dynamic.length-distance-stress.deflate.out.bin");

    // Error cases
    inflate_error_test(
        "dynamic.error.tree-size.code-lens.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate_error_test(
        "dynamic.error.tree-size.code-lens.tall.in.bin",
        "Too many symbols with code length 7. 3 symbols starting at 0x7E exceeds the specified number of bits");
    inflate_error_test(
        "dynamic.error.tree-size.literals.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate_error_test(
        "dynamic.error.tree-size.literals.tall.in.bin",
        "Too many symbols with code length 15. 3 symbols starting at 0x7FFE exceeds the specified number of bits");
    inflate_error_test(
        "dynamic.error.tree-size.distances.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate_error_test(
        "dynamic.error.tree-size.distances.tall.in.bin",
        "Too many symbols with code length 15. 3 symbols starting at 0x7FFE exceeds the specified number of bits");

    inflate_error_test("dynamic.error.code-lens-oob-repeat.begin.in.bin", "Code length repeat code encountered at beginning of data");
    inflate_error_test(
        "dynamic.error.code-lens-oob-repeat.end-prev.in.bin", "Code length repeat code specifies 6 repetitions, but only 5 codes remain");
    inflate_error_test(
        "dynamic.error.code-lens-oob-repeat.end-short.in.bin", "Zero repeat code specifies 10 repetitions, but only 9 codes remain");
    inflate_error_test(
        "dynamic.error.code-lens-oob-repeat.end-long.in.bin", "Zero repeat code specifies 138 repetitions, but only 1 codes remain");

    inflate_error_test(
        "dynamic.error.failed-lookup.code-lens.in.bin", "Input bit sequence 0x15 is not a valid Huffman code for the encoded table");
    inflate_error_test(
        "dynamic.error.failed-lookup.literals.short.in.bin", "Input bit sequence 0x6D is not a valid Huffman code for the encoded table");
    inflate_error_test(
        "dynamic.error.failed-lookup.literals.long.in.bin", "Input bit sequence 0xD16 is not a valid Huffman code for the encoded table");
    inflate_error_test(
        "dynamic.error.failed-lookup.distances.short.in.bin", "Input bit sequence 0x2B is not a valid Huffman code for the encoded table");
    inflate_error_test(
        "dynamic.error.failed-lookup.distances.long.in.bin", "Input bit sequence 0x58E is not a valid Huffman code for the encoded table");

    inflate_error_test("dynamic.error.invalid-symbol.286.in.bin", "Invalid symbol '286' from literal/length tree");
    inflate_error_test("dynamic.error.invalid-symbol.287.in.bin", "Invalid symbol '287' from literal/length tree");

    inflate_error_test(
        "dynamic.error.distance-oob.short.in.bin", "Compressed block has a distance '1' which exceeds the size of the window (0 bytes)");
    // inflate_error_test(
    //     "dynamic.error.distance-oob.long.deflate.in.bin",
    //     "Compressed block has a distance '65536' which exceeds the size of the window (65535 bytes)");
}

TEST_CASE("Inflate64CompressedDynamic", "[inflate64]")
{
    inflate64_test("dynamic.empty.in.bin", "dynamic.empty.out.bin");
    inflate64_test("dynamic.single.deflate64.in.bin", "dynamic.single.deflate64.out.bin");
    inflate64_test("dynamic.multiple.deflate64.in.bin", "dynamic.multiple.deflate64.out.bin");
    inflate64_test("dynamic.overlap.deflate64.in.bin", "dynamic.overlap.deflate64.out.bin");
    inflate64_test("dynamic.length-distance-stress.deflate64.in.bin", "dynamic.length-distance-stress.deflate64.out.bin");

    // Error cases
    inflate64_error_test(
        "dynamic.error.tree-size.code-lens.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate64_error_test(
        "dynamic.error.tree-size.code-lens.tall.in.bin",
        "Too many symbols with code length 7. 3 symbols starting at 0x7E exceeds the specified number of bits");
    inflate64_error_test(
        "dynamic.error.tree-size.literals.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate64_error_test(
        "dynamic.error.tree-size.literals.tall.in.bin",
        "Too many symbols with code length 15. 3 symbols starting at 0x7FFE exceeds the specified number of bits");
    inflate64_error_test(
        "dynamic.error.tree-size.distances.short.in.bin",
        "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits");
    inflate64_error_test(
        "dynamic.error.tree-size.distances.tall.in.bin",
        "Too many symbols with code length 15. 3 symbols starting at 0x7FFE exceeds the specified number of bits");

    inflate64_error_test("dynamic.error.code-lens-oob-repeat.begin.in.bin", "Code length repeat code encountered at beginning of data");
    inflate64_error_test(
        "dynamic.error.code-lens-oob-repeat.end-prev.in.bin", "Code length repeat code specifies 6 repetitions, but only 5 codes remain");
    inflate64_error_test(
        "dynamic.error.code-lens-oob-repeat.end-short.in.bin", "Zero repeat code specifies 10 repetitions, but only 9 codes remain");
    inflate64_error_test(
        "dynamic.error.code-lens-oob-repeat.end-long.in.bin", "Zero repeat code specifies 138 repetitions, but only 1 codes remain");

    inflate64_error_test(
        "dynamic.error.failed-lookup.code-lens.in.bin", "Input bit sequence 0x15 is not a valid Huffman code for the encoded table");
    inflate64_error_test(
        "dynamic.error.failed-lookup.literals.short.in.bin", "Input bit sequence 0x6D is not a valid Huffman code for the encoded table");
    inflate64_error_test(
        "dynamic.error.failed-lookup.literals.long.in.bin", "Input bit sequence 0xD16 is not a valid Huffman code for the encoded table");
    inflate64_error_test(
        "dynamic.error.failed-lookup.distances.short.in.bin", "Input bit sequence 0x2B is not a valid Huffman code for the encoded table");
    inflate64_error_test(
        "dynamic.error.failed-lookup.distances.long.in.bin", "Input bit sequence 0x58E is not a valid Huffman code for the encoded table");

    inflate64_error_test("dynamic.error.invalid-symbol.286.in.bin", "Invalid symbol '286' from literal/length tree");
    inflate64_error_test("dynamic.error.invalid-symbol.287.in.bin", "Invalid symbol '287' from literal/length tree");

    inflate64_error_test(
        "dynamic.error.distance-oob.short.in.bin", "Compressed block has a distance '1' which exceeds the size of the window (0 bytes)");
    inflate64_error_test(
        "dynamic.error.distance-oob.long.deflate64.in.bin",
        "Compressed block has a distance '65536' which exceeds the size of the window (65535 bytes)");
}

TEST_CASE("InflateCompressedStatic", "[inflate]")
{
    inflate_test("static.empty.in.bin", "static.empty.out.bin");
    // inflate_test("static.single.deflate.in.bin", "static.single.deflate.out.bin");
    // inflate_test("static.multiple.deflate.in.bin", "static.multiple.deflate.out.bin");
    // inflate_test("static.overlap.deflate.in.bin", "static.overlap.deflate.out.bin");
    // inflate_test("static.length-distance-stress.deflate.in.bin", "static.length-distance-stress.deflate.out.bin");

    inflate_error_test("static.error.invalid-symbol.286.in.bin", "Invalid symbol '286' from literal/length tree");
    inflate_error_test("static.error.invalid-symbol.287.in.bin", "Invalid symbol '287' from literal/length tree");

    inflate_error_test(
        "static.error.distance-oob.short.in.bin", "Compressed block has a distance '1' which exceeds the size of the window (0 bytes)");
    // inflate_error_test(
    //     "static.error.distance-oob.long.deflate.in.bin",
    //     "Compressed block has a distance '65536' which exceeds the size of the window (65535 bytes)");
}

TEST_CASE("Inflate64CompressedStatic", "[inflate64]")
{
    inflate64_test("static.empty.in.bin", "static.empty.out.bin");
    inflate64_test("static.single.deflate64.in.bin", "static.single.deflate64.out.bin");
    inflate64_test("static.multiple.deflate64.in.bin", "static.multiple.deflate64.out.bin");
    inflate64_test("static.overlap.deflate64.in.bin", "static.overlap.deflate64.out.bin");
    inflate64_test("static.length-distance-stress.deflate64.in.bin", "static.length-distance-stress.deflate64.out.bin");

    inflate64_error_test("static.error.invalid-symbol.286.in.bin", "Invalid symbol '286' from literal/length tree");
    inflate64_error_test("static.error.invalid-symbol.287.in.bin", "Invalid symbol '287' from literal/length tree");

    inflate64_error_test(
        "static.error.distance-oob.short.in.bin", "Compressed block has a distance '1' which exceeds the size of the window (0 bytes)");
    inflate64_error_test(
        "static.error.distance-oob.long.deflate64.in.bin",
        "Compressed block has a distance '65536' which exceeds the size of the window (65535 bytes)");
}

TEST_CASE("InflateCompressedMixed", "[inflate]")
{
    inflate_test("mixed.empty.in.bin", "mixed.empty.out.bin");
    inflate_test("mixed.simple.in.bin", "mixed.simple.out.bin");
    // inflate_test("mixed.overlap.deflate.in.bin", "mixed.overlap.deflate.out.bin");

    // Verify nothing bad happens if we call with no data
    {
        inflatelib_stream stream = {};
        REQUIRE(inflatelib_init(&stream) == INFLATELIB_OK);

        // All pointers/lengths should still be null from initialization above
        REQUIRE(inflatelib_inflate(&stream) == INFLATELIB_OK);
        REQUIRE(stream.next_in == nullptr);
        REQUIRE(stream.avail_in == 0);
        REQUIRE(stream.next_out == nullptr);
        REQUIRE(stream.avail_out == 0);
        REQUIRE(stream.total_in == 0);
        REQUIRE(stream.total_out == 0);

        inflatelib_destroy(&stream);
    }
}

TEST_CASE("Inflate64CompressedMixed", "[inflate64]")
{
    inflate64_test("mixed.empty.in.bin", "mixed.empty.out.bin");
    inflate64_test("mixed.simple.in.bin", "mixed.simple.out.bin");
    inflate64_test("mixed.overlap.deflate64.in.bin", "mixed.overlap.deflate64.out.bin");

    // Verify nothing bad happens if we call with no data
    {
        inflatelib_stream stream = {};
        REQUIRE(inflatelib_init(&stream) == INFLATELIB_OK);

        // All pointers/lengths should still be null from initialization above
        REQUIRE(inflatelib_inflate64(&stream) == INFLATELIB_OK);
        REQUIRE(stream.next_in == nullptr);
        REQUIRE(stream.avail_in == 0);
        REQUIRE(stream.next_out == nullptr);
        REQUIRE(stream.avail_out == 0);
        REQUIRE(stream.total_in == 0);
        REQUIRE(stream.total_out == 0);

        inflatelib_destroy(&stream);
    }
}

TEST_CASE("InflateRealWorldData", "[inflate]")
{
    // Tests a collection of files compressed with 7-Zip in an attempt to test scenarios that represent "real world data"
    // inflate_test("file.bin-write.deflate.exe.in.bin", "file.bin-write.deflate.exe.out.bin");
    // inflate_test("file.magna-carta.deflate.txt.in.bin", "file.magna-carta.deflate.txt.out.bin");
    // inflate_test("file.us-constitution.deflate.txt.in.bin", "file.us-constitution.deflate.txt.out.bin");
}

TEST_CASE("Inflate64RealWorldData", "[inflate64]")
{
    // Tests a collection of files compressed with 7-Zip in an attempt to test scenarios that represent "real world data"
    inflate64_test("file.bin-write.deflate64.exe.in.bin", "file.bin-write.deflate64.exe.out.bin");
    inflate64_test("file.magna-carta.deflate64.txt.in.bin", "file.magna-carta.deflate64.txt.out.bin");
    inflate64_test("file.us-constitution.deflate64.txt.in.bin", "file.us-constitution.deflate64.txt.out.bin");
}

TEST_CASE("InflateTruncation", "[inflate][inflate64]")
{
    auto doTestWorker = []<inflate_t inflateFunc>(const char* inputPath, const char* outputPath) {
        auto input = read_file(data_directory / inputPath);
        auto output = read_file(data_directory / outputPath);

        auto outputBuffer = std::make_unique<std::byte[]>(output.size);
        std::span<const std::byte> inputSpan = {input.buffer.get(), input.size};
        std::span<std::byte> outputSpan = {outputBuffer.get(), output.size};

        inflatelib::stream stream;
        REQUIRE((stream.*inflateFunc)(inputSpan, outputSpan) == true); // 'true' means not done yet

        REQUIRE(inputSpan.empty());
        REQUIRE(outputSpan.empty());

        // Calling again should immediately return true since there's no new input
        outputSpan = {outputBuffer.get(), output.size}; // Reuse buffer to test that we don't write new data
        REQUIRE((stream.*inflateFunc)(inputSpan, outputSpan) == true);
        REQUIRE(outputSpan.size() == output.size);

        REQUIRE(std::memcmp(outputBuffer.get(), output.buffer.get(), output.size) == 0);
    };

    auto doTest = [&](const char* inputPath, const char* outputPath) {
        doTestWorker.operator()<&inflatelib::stream::inflate>(inputPath, outputPath);
        doTestWorker.operator()<&inflatelib::stream::inflate64>(inputPath, outputPath);
    };

    doTest("truncated.uncompressed.block.in.bin", "truncated.uncompressed.block.out.bin");
    doTest("truncated.uncompressed.no-bfinal.in.bin", "truncated.uncompressed.no-bfinal.out.bin");
    doTest("truncated.dynamic.block.in.bin", "truncated.dynamic.block.out.bin");
    doTest("truncated.dynamic.no-bfinal.in.bin", "truncated.dynamic.no-bfinal.out.bin");
    doTest("truncated.static.block.in.bin", "truncated.static.block.out.bin");
    doTest("truncated.static.no-bfinal.in.bin", "truncated.static.no-bfinal.out.bin");
}

TEST_CASE("InflateExtraData", "[inflate][inflate64]")
{
    auto doTestWorker = []<inflate_t inflateFunc>(const char* inputPath, const char* outputPath) {
        auto input = read_file(data_directory / inputPath);
        auto output = read_file(data_directory / outputPath);

        auto outputBuffer = std::make_unique<std::byte[]>(output.size);
        std::span<const std::byte> inputSpan = {input.buffer.get(), input.size};
        std::span<std::byte> outputSpan = {outputBuffer.get(), output.size};

        inflatelib::stream stream;
        REQUIRE((stream.*inflateFunc)(inputSpan, outputSpan) == false); // 'false' means we've decoded all data
        REQUIRE(!inputSpan.empty());                               // Since there's extra data at the end
        REQUIRE(outputSpan.empty());

        // Calling again should immediately return false since we've already hit end of stream
        outputSpan = {outputBuffer.get(), output.size}; // Reuse buffer to test that we don't write new data
        REQUIRE((stream.*inflateFunc)(inputSpan, outputSpan) == false);
        REQUIRE(outputSpan.size() == output.size);

        REQUIRE(std::memcmp(outputBuffer.get(), output.buffer.get(), output.size) == 0);
    };

    auto doTest = [&](const char* inputPath, const char* outputPath) {
        doTestWorker.operator()<&inflatelib::stream::inflate>(inputPath, outputPath);
        doTestWorker.operator()<&inflatelib::stream::inflate64>(inputPath, outputPath);
    };

    doTest("extra.uncompressed.in.bin", "extra.uncompressed.out.bin");
    doTest("extra.dynamic.in.bin", "extra.dynamic.out.bin");
    doTest("extra.static.in.bin", "extra.static.out.bin");
}

TEST_CASE("InflateReset", "[inflate][inflate64]")
{
    // Simple initialization test
    inflatelib::stream stream{nullptr};
    REQUIRE(!stream);

    stream = inflatelib::stream{};
    REQUIRE(stream);

    file_contents input = {};
    file_contents output = {};
    auto doInflate = [&](std::size_t inputSize = 0) {
        // We don't require EOF on error, so default size not that important
        std::size_t outputBufferSize = output.size ? output.size : 0x20000;
        auto outputBuffer = std::make_unique<std::byte[]>(outputBufferSize);

        inputSize = inputSize ? inputSize : input.size;

        // NOTE: We provide buffers the exact size of the input/output, so we only need one call
        std::span<const std::byte> inputSpan = {input.buffer.get(), inputSize};
        std::span<std::byte> outputSpan = {outputBuffer.get(), outputBufferSize};
        auto result = stream.try_inflate64(inputSpan, outputSpan);
        if (!output.size)
        {
            REQUIRE(result < INFLATELIB_OK);
        }
        else if (inputSize == input.size)
        {
            REQUIRE(result == INFLATELIB_EOF);
            REQUIRE(inputSpan.empty());
            REQUIRE(outputSpan.empty()); // Should have consumed all the space
            REQUIRE(std::memcmp(outputBuffer.get(), output.buffer.get(), output.size) == 0);
        }
        else
        {
            REQUIRE(result == INFLATELIB_OK);
        }
    };

    // The scenarios that this test:
    //  1.  'reset' after EOF allows us to read new data
    //  2.  'reset' after error allows us to read new data
    //  3.  'reset' in the middle of a stream allows us to read new data
    //  4.  'reset' after reading Deflate64 data allows us to read Deflate data & vice-versa

    // Test 1: Reset after EOF
    // NOTE: For all of these tests, we read archives that use Huffman tables to ensure proper re-use
    input = read_file(data_directory / "dynamic.single.deflate64.in.bin"); // Just a simple, small file to test
    output = read_file(data_directory / "dynamic.single.deflate64.out.bin");
    doInflate();
    stream.reset();

    input = read_file(data_directory / "static.single.deflate64.in.bin");
    output = read_file(data_directory / "static.single.deflate64.out.bin");
    doInflate();
    stream.reset();

    // Test 2: Reset after error
    input = read_file(data_directory / "dynamic.error.distance-oob.long.deflate64.in.bin");
    output = {}; // Error; no output
    doInflate();
    stream.reset();

    input = read_file(data_directory / "static.multiple.deflate64.in.bin");
    output = read_file(data_directory / "static.multiple.deflate64.out.bin");
    doInflate();
    stream.reset();

    // Test 3: Reset in the middle of a stream
    input = read_file(data_directory / "mixed.overlap.deflate64.in.bin");
    output = read_file(data_directory / "mixed.overlap.deflate64.out.bin");
    doInflate(256);
    stream.reset();

    input = read_file(data_directory / "dynamic.multiple.deflate64.in.bin");
    output = read_file(data_directory / "dynamic.multiple.deflate64.out.bin");
    doInflate();
    stream.reset();

    // Test 4: Reset after reading Deflate64 data and then reading Deflate data
    // NOTE: Previous test read Deflate64 data
    // TODO: Read deflate data

    // Test 4, part 2: Reset after reading Deflate data and then reading Deflate64 data
    // NOTE: Previous test read Deflate data
    input = read_file(data_directory / "static.overlap.deflate64.in.bin");
    output = read_file(data_directory / "static.overlap.deflate64.out.bin");
    doInflate();
    stream.reset();
}
