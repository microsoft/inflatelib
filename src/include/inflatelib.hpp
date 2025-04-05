/*
 * C++ Wrappers/helpers around the deflate64 functions/types
 */
#ifndef INFLATELIB_HPP
#define INFLATELIB_HPP

#include "inflatelib.h"

#include <cassert>
#include <cstddef>
#include <new>
#include <span>
#include <stdexcept>

// MSVC has historically defined __cplusplus to be a value that is not consistent with the C++ standard being used
#if _MSVC_LANG > __cplusplus
#define INFLATELIB_CPP_VERSION _MSVC_LANG
#else
#define INFLATELIB_CPP_VERSION __cplusplus
#endif

// MSVC does not yet implement the optimization for [[no_unique_address]], so use the compiler-specific attribute
#if _MSC_VER >= 1929
#define INFLATELIB_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#define INFLATELIB_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif

#ifndef INFLATELIB_NO_UNIQUE_ADDRESS
// Default to no definition
#define INFLATELIB_NO_UNIQUE_ADDRESS
#endif

namespace inflatelib
{
    struct stream
    {
        stream()
        {
            init();
        }

        stream(void* userData, inflatelib_alloc alloc, inflatelib_free free)
        {
            m_stream.user_data = userData;
            m_stream.alloc = alloc;
            m_stream.free = free;
            init();
        }

        // Constructor that allows for no initialization of the underlying inflatelib_stream. This is primarily useful
        // for scenarios such as global variables or struct members whose initialization should be delayed until needed
        stream(std::nullptr_t) noexcept
        {
        }

        ~stream()
        {
            // NOTE: Okay to call even in the moved-from state since all fields will be null
            [[maybe_unused]] auto result = ::inflatelib_destroy(&m_stream);
            assert(result == INFLATELIB_OK);
        }

        // Internal state is not copyable
        stream(const stream&) = delete;
        stream& operator=(const stream&) = delete;

        // All data inside the inflatelib_stream can safely be relocated. Clearing all values to zero is sufficient to
        // avoid issues when destroying the stream
        stream(stream&& other) noexcept : m_stream(other.m_stream)
        {
            other.m_stream = {};
        }

        stream& operator=(stream&& other) noexcept
        {
            if (this != &other)
            {
                // Destroy the current stream
                // NOTE: Okay to call even in the moved-from state since all fields will be null
                ::inflatelib_destroy(&m_stream);

                // Move the other stream into this one
                m_stream = other.m_stream;
                other.m_stream = {};
            }
            return *this;
        }

        void reset()
        {
            if (auto result = ::inflatelib_reset(&m_stream); result != INFLATELIB_OK)
            {
                throw_error(result);
            }
        }

        [[nodiscard]] bool inflate(std::span<const std::byte>& input, std::span<std::byte>& output)
        {
            auto result = try_inflate(input, output);
            if (result < INFLATELIB_OK)
            {
                throw_error(result);
            }

            return result == INFLATELIB_OK; // Return true if the caller should keep calling
        }

        [[nodiscard]] int try_inflate(std::span<const std::byte>& input, std::span<std::byte>& output) noexcept
        {
            m_stream.next_in = input.data();
            m_stream.avail_in = input.size_bytes();
            m_stream.next_out = output.data();
            m_stream.avail_out = output.size_bytes();

            auto result =  ::inflatelib_inflate(&m_stream);

            // Update the caller based on what was consumed/written
            input = {static_cast<const std::byte*>(m_stream.next_in), m_stream.avail_in};
            output = {static_cast<std::byte*>(m_stream.next_out), m_stream.avail_out};

            return result;
        }

        [[nodiscard]] bool inflate64(std::span<const std::byte>& input, std::span<std::byte>& output)
        {
            auto result = try_inflate64(input, output);
            if (result < INFLATELIB_OK)
            {
                throw_error(result);
            }

            return result == INFLATELIB_OK; // Return true if the caller should keep calling
        }

        [[nodiscard]] int try_inflate64(std::span<const std::byte>& input, std::span<std::byte>& output) noexcept
        {
            m_stream.next_in = input.data();
            m_stream.avail_in = input.size_bytes();
            m_stream.next_out = output.data();
            m_stream.avail_out = output.size_bytes();

            auto result =  ::inflatelib_inflate64(&m_stream);

            // Update the caller based on what was consumed/written
            input = {static_cast<const std::byte*>(m_stream.next_in), m_stream.avail_in};
            output = {static_cast<std::byte*>(m_stream.next_out), m_stream.avail_out};

            return result;
        }

        [[nodiscard]] inflatelib_stream* get() noexcept
        {
            return &m_stream;
        }

        [[nodiscard]] const inflatelib_stream* get() const noexcept
        {
            return &m_stream;
        }

        [[nodiscard]] const char* error_msg() const noexcept
        {
            return m_stream.error_msg;
        }

        // Convenience method to check if the stream has been initialized
        operator bool() const noexcept
        {
            return m_stream.internal != nullptr;
        }

    private:

        void init()
        {
            if (auto result = ::inflatelib_init(&m_stream); result != INFLATELIB_OK)
            {
                throw_error(result);
            }
        }

        [[noreturn]] void throw_error(int result)
        {
            assert(result < 0); // Likely EOF, but wrong conditional

            auto msg = m_stream.error_msg;
            if (!msg)
            {
                msg = "unknown failure in inflatelib stream";
            }

            switch (result)
            {
            case INFLATELIB_ERROR_ARG:
                throw std::invalid_argument(msg);
            default:
                assert(false); // Should be unreachable
                [[fallthrough]];
            case INFLATELIB_ERROR_DATA:
                throw std::runtime_error(msg);
            case INFLATELIB_ERROR_OOM:
                throw std::bad_alloc();
            }
        }

        inflatelib_stream m_stream = {};
    };
}

#endif // INFLATELIB_HPP