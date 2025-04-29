
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

template <typename T>
struct le_value
{
    static_assert(std::is_arithmetic_v<T>, "Only arithmetic types are supported");

    le_value() = delete; // Data read from memory; instances acquired via 'reinterpret_cast'

    T get() const noexcept
    {
        if constexpr (std::endian::native == std::endian::little)
        {
            return internal.value;
        }
        else
        {
            auto bytes = internal.bytes;
            std::reverse(bytes.begin(), bytes.end());
            return std::bit_cast<T>(bytes);
        }
    }

private:
    union
    {
        T value;
        std::array<std::byte, sizeof(T)> bytes;
    } internal;
};

using le_u16 = le_value<std::uint16_t>;
using le_u32 = le_value<std::uint32_t>;

// These structures are defined by the ZIP file format; the compiler shouldn't add its own padding
#pragma pack(push)
#pragma pack(1)
struct end_of_central_directory
{
    std::uint8_t signature[4];
    le_u16 disk_number;
    le_u16 disk_with_cd;
    le_u16 cd_records_on_disk;
    le_u16 cd_records;
    le_u32 cd_size;
    le_u32 cd_offset;
    le_u16 comment_length;

    bool valid() const noexcept
    {
        static constexpr const std::array<std::uint8_t, 4> expected = {0x50, 0x4B, 0x05, 0x06};
        return std::memcmp(signature, expected.data(), 4) == 0;
    }

    std::string_view comment() const noexcept
    {
        return {reinterpret_cast<const char*>(this + 1), comment_length.get()};
    }
};
static_assert(sizeof(end_of_central_directory) == 22);

struct central_directory_file_header
{
    std::uint8_t signature[4];
    le_u16 version;
    le_u16 min_version;
    le_u16 bit_flag;
    le_u16 compression_method;
    le_u16 mod_time;
    le_u16 mod_date;
    le_u32 crc32;
    le_u32 compressed_size;
    le_u32 uncompressed_size;
    le_u16 file_name_length;
    le_u16 extra_field_length;
    le_u16 file_comment_length;
    le_u16 disk_number_start;
    le_u16 internal_file_attribute;
    le_u32 external_file_attributes;
    le_u32 local_file_header_offset;

    bool valid() const noexcept
    {
        static constexpr const std::array<std::uint8_t, 4> expected = {0x50, 0x4B, 0x01, 0x02};
        return std::memcmp(signature, expected.data(), 4) == 0;
    }

    std::string_view file_name() const noexcept
    {
        return {reinterpret_cast<const char*>(this + 1), file_name_length.get()};
    }

    std::span<const std::uint8_t> extra_field() const noexcept
    {
        return {reinterpret_cast<const std::uint8_t*>(this + 1) + file_name_length.get(), extra_field_length.get()};
    }

    std::string_view file_comment() const noexcept
    {
        return {reinterpret_cast<const char*>(this + 1) + file_name_length.get() + extra_field_length.get(), file_comment_length.get()};
    }

    const central_directory_file_header* next() const noexcept
    {
        return reinterpret_cast<const central_directory_file_header*>(
            reinterpret_cast<const std::uint8_t*>(this + 1) + file_name_length.get() + extra_field_length.get() +
            file_comment_length.get());
    }
};
static_assert(sizeof(central_directory_file_header) == 46);

struct local_file_header
{
    std::uint8_t signature[4];
    le_u16 version;
    le_u16 bit_flag;
    le_u16 compression_method;
    le_u16 mod_time;
    le_u16 mod_date;
    le_u32 crc32;
    le_u32 compressed_size;
    le_u32 uncompressed_size;
    le_u16 file_name_length;
    le_u16 extra_field_length;

    bool valid() const noexcept
    {
        static constexpr const std::array<std::uint8_t, 4> expected = {0x50, 0x4B, 0x03, 0x04};
        return std::memcmp(signature, expected.data(), 4) == 0;
    }

    std::string_view file_name() const noexcept
    {
        return {reinterpret_cast<const char*>(this + 1), file_name_length.get()};
    }

    std::span<const std::uint8_t> extra_field() const noexcept
    {
        return {reinterpret_cast<const std::uint8_t*>(this + 1) + file_name_length.get(), extra_field_length.get()};
    }

    std::size_t size() const noexcept
    {
        return sizeof(*this) + file_name_length.get() + extra_field_length.get();
    }
};
static_assert(sizeof(local_file_header) == 30);
#pragma pack(pop)

static const char* compression_method(std::uint16_t method) noexcept
{
    switch (method)
    {
    case 0:
        return "Stored (no compression)";
    case 1:
        return "Shrunk";
    case 2:
        return "Reduced with compression factor 1";
    case 3:
        return "Reduced with compression factor 2";
    case 4:
        return "Reduced with compression factor 3";
    case 5:
        return "Reduced with compression factor 4";
    case 6:
        return "Imploded";
    case 7:
        return "Reserved";
    case 8:
        return "Deflated";
    case 9:
        return "Enhanced Deflated (Deflate64)";
    default:
        return "Unknown compression method";
    }
}

void print_usage()
{
    std::println(R"^-^(
USAGE
    zip-extract <path>

DESCRIPTION
    "Extracts" the file data portion as-is from all files in the specified zip file. This outputs text in a format that
    can be used with the 'bin-write' executable to reproduce each individual file's contents as it appears in the zip
    file.

ARGUMENTS
    path    The path to the input zip file.
)^-^");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::println("ERROR: Expected path to a zip file");
        return print_usage(), 1;
    }

    FILE* file;
    if (fopen_s(&file, argv[1], "rb") != 0)
    {
        std::println("ERROR: Failed to open file '{}'", argv[1]);
        return print_usage(), 1;
    }

    // Figure out the size of the file
    if (fseek(file, 0, SEEK_END) != 0)
    {
        std::println("ERROR: Failed to seek to end of file");
        return 1;
    }

    auto fileSize = ftell(file);
    if (fileSize < static_cast<long>(sizeof(end_of_central_directory)))
    {
        std::println("ERROR: File is too small to contain a ZIP central directory");
        return 1;
    }

    // The EOCD has a maximum size of 65557 bytes (0xFFFF + 22 bytes). It's better to read in powers of two, so we
    // choose a buffer size of 128 KiB
    long cd_buffer_size = 128 * 1024;
    auto cd_buffer = std::make_unique<std::uint8_t[]>(cd_buffer_size);

    // Read the last 128 KiB of the file and then search for the EOCD. Hopefully, given our large buffer size, we've
    // also read the central directory into memory
    auto readSize = std::min(fileSize, cd_buffer_size);
    auto readFileOffset = fileSize - readSize;
    if (fseek(file, readFileOffset, SEEK_SET) != 0)
    {
        std::println("ERROR: Failed to seek file");
        return 1;
    }

    if (fread(cd_buffer.get(), 1, static_cast<std::size_t>(readSize), file) != static_cast<std::size_t>(readSize))
    {
        std::println("ERROR: Failed to read file");
        return 1;
    }

    end_of_central_directory* eocd = nullptr;
    for (long bufferOffset = readSize - sizeof(end_of_central_directory); bufferOffset >= 0; --bufferOffset)
    {
        eocd = reinterpret_cast<end_of_central_directory*>(cd_buffer.get() + bufferOffset);
        if (eocd->valid())
        {
            break;
        }
    }

    if (!eocd || !eocd->valid())
    {
        std::println("ERROR: Failed to find the central directory");
        return 1;
    }

    // Cache these values now as they'll be invalidated if we need to read more data
    auto cdSize = eocd->cd_size.get();
    auto cdFileOffset = eocd->cd_offset.get();
    eocd = nullptr; // No longer valid to read from

    if (cdSize < sizeof(central_directory_file_header))
    {
        std::println("ERROR: Central directory is too small to contain a file header");
        return 1;
    }
    else if (cdFileOffset + cdSize > static_cast<std::uint32_t>(fileSize))
    {
        std::println("ERROR: Central directory extends beyond the end of the file");
        return 1;
    }

    bool cdNeedsRead = false;
    if (cdSize > static_cast<std::uint32_t>(cd_buffer_size))
    {
        // Round up size to a power of two
        cd_buffer_size = std::bit_ceil(cdSize);
        cd_buffer = std::make_unique<std::uint8_t[]>(cd_buffer_size);
        cdNeedsRead = true;
    }
    else if (cdFileOffset < static_cast<std::uint32_t>(readFileOffset))
    {
        cdNeedsRead = true;
    }

    if (cdNeedsRead)
    {
        readFileOffset = cdFileOffset;
        readSize = std::min(cd_buffer_size, fileSize - readFileOffset);
        if (fseek(file, readFileOffset, SEEK_SET) != 0)
        {
            std::println("ERROR: Failed to seek file");
            return 1;
        }
        else if (fread(cd_buffer.get(), 1, static_cast<std::size_t>(readSize), file) != static_cast<std::size_t>(readSize))
        {
            std::println("ERROR: Failed to read file");
            return 1;
        }
    }

    // We want the central directory to remain in memory, so we'll need a separate buffer for file contents
    // Large enough to hold any local file header, rounded up to next power of two
    static constexpr const std::size_t file_buffer_size = 256 * 1024;
    auto file_buffer = std::make_unique<std::uint8_t[]>(file_buffer_size);

    auto cdStartBufferOffset = cdFileOffset - readFileOffset;
    auto firstEntry = reinterpret_cast<const central_directory_file_header*>(cd_buffer.get() + cdStartBufferOffset);
    auto currentEntry = firstEntry;
    while (true)
    {
        // At this point we've already validated that there's _at least_ enough space for a full file header. Make sure
        // that any extra data in the header (file name, etc.) is also within the buffer
        if (!currentEntry->valid())
        {
            std::println("ERROR: Invalid central directory entry");
            return 1;
        }

        auto nextEntry = currentEntry->next();
        auto nextEntryCdOffset = reinterpret_cast<const std::uint8_t*>(nextEntry) - reinterpret_cast<const std::uint8_t*>(firstEntry);
        if (static_cast<std::size_t>(nextEntryCdOffset) > cdSize)
        {
            std::println("ERROR: Central directory entry extends beyond the end of the central directory");
            return 1;
        }

        // Before reading data, we need to seek, which starts with the local file header
        if (fseek(file, currentEntry->local_file_header_offset.get(), SEEK_SET) != 0)
        {
            std::println("ERROR: Failed to seek to file {}", currentEntry->file_name());
            return 1;
        }

        // Read just enough data to know the size of both the local header and compressed size
        if (fread(file_buffer.get(), 1, sizeof(local_file_header), file) != sizeof(local_file_header))
        {
            std::println("ERROR: Failed to read local file header for {}", currentEntry->file_name());
            return 1;
        }

        auto localHeader = reinterpret_cast<local_file_header*>(file_buffer.get());
        if (!localHeader->valid())
        {
            std::println("ERROR: Invalid local file header for {}", currentEntry->file_name());
            return 1;
        }

        // Figure out how much data we need to read for both the header and the contents
        // NOTE: We've already read the local file header & have "seeked" past it; don't read again
        auto totalSize = localHeader->size() + localHeader->compressed_size.get();
        auto dataReadSize = std::min(totalSize - sizeof(local_file_header), file_buffer_size - sizeof(local_file_header));
        if (fread(file_buffer.get() + sizeof(local_file_header), 1, dataReadSize, file) != dataReadSize)
        {
            std::println("ERROR: Failed to read local file header/data for {}", currentEntry->file_name());
            return 1;
        }

        dataReadSize += sizeof(local_file_header); // Since the buffer also contains the start of the header

        // Sanity check that the local header and CD header are in-sync. Realistically. Most of this data doesn't
        // matter, however mis-matches in some of this data result in ambiguities in the data we need to read/report
        if (localHeader->file_name() != currentEntry->file_name())
        {
            std::println("ERROR: File name mismatch for {}", currentEntry->file_name());
            std::println("NOTE: Local header name is {}", localHeader->file_name());
            return 1;
        }
        else if (localHeader->compressed_size.get() != currentEntry->compressed_size.get())
        {
            std::println("ERROR: Compressed size mismatch for {}", currentEntry->file_name());
            return 1;
        }
        else if (localHeader->compression_method.get() != currentEntry->compression_method.get())
        {
            std::println("ERROR: Compression method mismatch for {}", currentEntry->file_name());
            return 1;
        }

        std::println("# Data for file: {}", currentEntry->file_name());
        std::println("# This file is compressed using: {}", compression_method(localHeader->compression_method.get()));
        std::println("# Compressed size: {} bytes", localHeader->compressed_size.get());
        std::println("# Uncompressed size: {} bytes", localHeader->uncompressed_size.get());

        static constexpr std::uint8_t line_size = 32;
        auto offset = static_cast<long>(localHeader->size());
        std::uint8_t bytesInLastOutput = 0;
        const char* prefix = "";
#ifdef _DEBUG
        std::uint32_t totalBytesWritten = 0;
#endif

        // NOTE: At this point, 'localHeader' is assumed to be an invalid pointer since we may need to read more data
        while (true)
        {
            auto ptr = reinterpret_cast<const std::uint8_t*>(file_buffer.get()) + offset;
            std::size_t outputSize = dataReadSize - offset;
            while (outputSize > 0)
            {
                auto lineSize = std::min(outputSize, static_cast<std::size_t>(line_size - bytesInLastOutput));
                outputSize -= lineSize;
                bytesInLastOutput += static_cast<std::uint8_t>(lineSize);
#ifdef _DEBUG
                totalBytesWritten += static_cast<std::uint32_t>(lineSize);
#endif
                for (std::size_t i = 0; i < lineSize; ++i)
                {
                    std::print("{}{:02X}", prefix, *ptr++);
                    prefix = " ";
                }

                if (bytesInLastOutput == line_size)
                {
                    bytesInLastOutput = 0;
                    prefix = "";
                    std::println();
                }
            }

            totalSize -= dataReadSize;
            if (totalSize <= 0)
            {
                break;
            }

            // Read more data
            dataReadSize = std::min(totalSize, file_buffer_size);
            offset = 0;
            if (fread(file_buffer.get(), 1, dataReadSize, file) != dataReadSize)
            {
                std::println("ERROR: Failed to read file data for {}", currentEntry->file_name());
                return 1;
            }
        }

#ifdef _DEBUG
        assert(totalBytesWritten == currentEntry->compressed_size.get());
#endif
        std::println("\n\n");

        if (static_cast<std::size_t>(nextEntryCdOffset) == cdSize)
        {
            break;
        }
        currentEntry = nextEntry;
    }
}
