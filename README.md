
# About

InflateLib is a Deflate/Deflate64 decompression library written in C based off [dotnet's System.IO.Compression implementation](https://github.com/dotnet/runtime/tree/main/src/libraries/System.IO.Compression/src/System/IO/Compression/DeflateManaged).
The API is modeled after that of [zlib](https://zlib.net/).


# Links

* [Contributing to InflateLib](./CONTRIBUTING.md)
* [Reporting security issues](./SECURITY.md)
* [Using the library](#usage)
* [Frequently asked questions](#faq)

# Usage

Before getting started, you will need to ensure that InflateLib is accessible to your build system.
If you are using CMake, you will want to find the inflatelib package and "link" against its target.

```cmake
find_package(inflatelib REQUIRED)

target_link_libraries(myapp PRIVATE inflatelib::inflatelib)
```

Using the InflateLib library should be familiar to anyone who has used zlib before.
To start, you will need to include the [InflateLib header](src/include/inflatelib.h).

```C
#include <inflatelib.h>
```

Next, you need to declare an `inflatelib_stream` variable and initialize it with `inflatelib_init`.
Once you are finished with the stream, you can clean up its resources by calling `inflatelib_destroy`.

```C
inflatelib_stream stream = { 0 };
if (inflatelib_init(&stream) != INFLATELIB_OK)
{
    handle_error(stream.error_msg);
    return 1;
}

/* Use the stream */

inflatelib_destroy(&stream);
```

You can additionally provide your own memory management routines used for allocating and deallocating memory if you desire.

```C
/* Elsewhere in the project/file */
void* my_alloc(void* userData, size_t bytes, size_t alignment);
void my_free(void* userData, void* allocatedPtr, size_t bytes, size_t alignment);

/* Later on */
inflatelib_stream stream = { 0 };
stream.user_data = myAllocator;
stream.alloc = my_alloc;
stream.free = my_free;
if (inflatelib_innit(&stream) != INFLATELIB_OK) /* ... */
```

> [!TIP]
> If you are using something like a monotonic allocator with the `inflatelib_stream`, it is not strictly necessary that you call `inflatelib_destroy` when you are done using it.
> All memory allocated by `inflatelib_stream` uses the allocation functions provided to it, making it possible to "blink" its memory out of existence.

At this point, you can begin decompressing data.

```C
int result = INFLATELIB_OK;
char* inputBuffer, *outputBuffer; /* Initialization not shown */
size_t inputSize, outputBufferSize; /* Initialization not shown */

/* Assume the whole input stream is in memory. Otherwise we need to update these values and read new data as necessary
 * inside the loop */
stream.next_in = inputBuffer;
stream.avail_in = inputSize;

while (result != INFLATELIB_EOF)
{
    stream.next_out = outputBuffer;
    stream.avail_out = outputBufferSize;

    result = inflatelib_inflate64(&stream);
    if (result < INFLATELIB_OK)
    {
        handle_error(stream.error_msg);
        inflatelib_destroy(&stream);
        return 1;
    }

    handle_output(outputBuffer, outputBufferSize - stream.avail_out);
}

inflatelib_destroy(&stream);
return 0;
```

Once the end of the stream is reached (`INFLATELIB_EOF`), the `inflatelib_stream` will be in a terminal state.
That means that any subsequent calls will immediately return with `INFLATELIB_EOF` and not consume or write any additional data.
If you need to decompress multiple streams of data (e.g. multiple files), you can call `inflatelib_reset` to reset the stream back to its initial state.

```C
/* Data declared/initialized as before */

while (have_more_input)
{
    inputSize = read_next_input(inputBuffer); /* Assume large enough to read one entire stream's worth of input */
    stream.next_in = inputBuffer;
    stream.avail_in = inputSize;

    while (result != INFLATELIB_EOF)
    {
        /* Same loop as before */
    }

    inflatelib_reset(&stream);
}

inflatelib_destroy(&stream);
return 0;
```

In the example code above, we are decompressing Deflate64 encoded data, as indicated by the call to `inflatelib_inflate64`.
Deflation of Deflate encoded data is done by calling `inflatelib_inflate`.
Alternating between these two functions will cause an error _unless_ the stream is reset in between calls.
For example, the following is valid:

```C
result = inflatelib_inflate(&stream);
/* More calls to inflatelib_inflate are allowed, but calls to inflatelib_inflate64 will error */

inflatelib_reset(&stream);
/* At this point, either inflatelib_inflate or inflatelib_inflate64 can be called */

result = inflatelib_inflate64(&stream);
/* More calls to inflatelib_inflate64 are allowed, but calls to inflatelib_inflate will error */
```

## C++ Interface

If you are authoring a C++ application or library, you can alternatively include [`<inflatelib.hpp>`](src/include/inflatelib.hpp) for a more "C++ friendly" interface.
When using the C++ interface, you will use the `inflatelib::stream` type in place of `inflatelib_stream`.
The call to `inflatelib_init` is done in the constructor and the call to `inflatelib_destroy` is done in the destructor.
As such, if you wish to provide custom memory management routines, these should be passed to the constructor.
Additionally, if you wish to delay initialization of the stream (e.g. for global variables, etc.), you can construct with `nullptr`.

```C++
// Use the default allocation & deallocation functions
inflatelib::stream s;

// Use custom allocation & deallocation functions
inflatelib::stream s2(myAllocator, &my_alloc, &my_free);

// Delay initialization
inflatelib::stream s3(nullptr);
s3 = inflatelib::stream(); // s3 is now initialized and can be used to inflate data
```

The inflation functions are then exposed as member functions off the `inflatelib::stream` type.
These functions take in two references to `std::span<std::byte>` that are used to set the input/output buffers and are updated on function exit to reflect the unused input/output buffer.
For example, the final example from above when everything is put together, becomes:

```C++
std::span<const std::byte> input;
std::span<std::byte> output; /* Initialization not shown */

inflatelib::stream stream;
while (have_more_input)
{
    input = read_more_input(); /* Assume result large enough to hold one entire stream's worth of input */

    for (bool keepGoing = true; keepGoing; )
    {
        auto outputCopy = output;
        keepGoing = stream.inflate(input, outputCopy);
        handle_output(output.subspan(0, output.size() - outputCopy.size()));
    }

    stream.reset();
}
```

You will notice that there is no error handling in the above code.
When using the C++ interface, errors are thrown as exceptions: `std::invalid_argument` for `INFLATELIB_ERROR_ARG`, `std::bad_alloc` for `INFLATELIB_ERROR_OOM`, and `std::runtime_error` for `INFLATELIB_ERROR_DATA`.
If you don't wish for the inflation functions to throw exceptions, you can instead use the `try_inflate`/`try_inflate64` functions, which return the `int` result, unmodified.
There is no non-throwing alternative to the constructor.

# FAQ

> Q: Why is this library written in C? Why not a memory safe language?

The primary purpose of this library is to be easily integratable into other existing libraries and applications written in C.
If memory safe languages is a concern or requirement for you, several Deflate64 libraries already exist in such languages.
That said, this library has an extensive test suite, compiles and runs against both Address and Undefined Behavior sanitizer, and compiles a libFuzzer executable that is run nightly.

> Q: How does this library differ from zlib?

Zlib currently only has experimental, minimally tested support for Deflate64 that is difficult to enable in a consuming project.
The official stance of the zlib project is that the benefit of Deflate64 is minimal compared to the complexity of implementation and they have no intention of adding official support to the library at this time.
On top of that, this experimental support only supports "inflate back" semantics, which is not compatible with the design of some libraries.

> Q: If the primary purpose was to create a Deflate64 decompression library, why do you also support Deflate decompression?

While initially written to support Deflate64 only, it turns out that the differences between Deflate and Deflate64 are very minor: just a few modifications to a couple lookup tables.
Multiplexing between these two is trivial and the overhead of doing so is negligible.
Supporting both allows consuming applications to reduce the number of dependencies if all they need is decompression.

> Q: Does this library support deflation (compression)? Are there any plans to add support?

No, compression is not currently supported by the library, nor are there plans to add support in the future.
The proprietary nature of Deflate64 and the minimal gains over Deflate make this not worth pursuing at this time.
If you need compression support for Deflate, we recommend using a separate library such as zlib.
