
#include <memory>

#include <inflatelib.h>

#ifdef _WIN32
#define FUZZ_EXPORT __declspec(dllexport)
#define FUZZ_CALLCONV __cdecl
#else
#define FUZZ_EXPORT
#define FUZZ_CALLCONV
#endif

extern "C" FUZZ_EXPORT int FUZZ_CALLCONV LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    inflatelib_stream stream = {};
    if (inflatelib_init(&stream) < INFLATELIB_OK)
    {
        return -1;
    }

    static constexpr std::size_t buffer_size = 65536;
    auto buffer = std::make_unique<uint8_t[]>(buffer_size);

    // This is all the input there will ever be, so we only need to set it once
    stream.next_in = data;
    stream.avail_in = size;

    int result = 0;
    while (stream.avail_in > 0)
    {
        // We don't care about the data written, so just make the entire buffer available for output on each iteration
        stream.next_out = buffer.get();
        stream.avail_out = buffer_size;

        auto inflateResult = inflatelib_inflate64(&stream);
        if (inflateResult == INFLATELIB_EOF)
        {
            break;
        }
        else if (inflateResult < INFLATELIB_OK)
        {
            result = -1;
            break;
        }
    }

    inflatelib_destroy(&stream);

    return result;
}
