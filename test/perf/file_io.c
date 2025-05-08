
#include "pch.h"

#include "file_io.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define PATH_SEPARATOR_CHR '\\'
#define PATH_SEPARATOR_STR "\\"
#include <Windows.h>
#else
#define PATH_SEPARATOR_CHR '/'
#define PATH_SEPARATOR_STR "/"
#include <limits.h>
#include <stdlib.h>
#endif

char* resolve_test_file_path(const char* filename)
{
    char* result = NULL;
    size_t filenameLen = strlen(filename);
    size_t testsPathLen = 0;

#ifdef _WIN32
    DWORD moduleLen;
    char buffer[MAX_PATH];

    moduleLen = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (!moduleLen || (moduleLen == MAX_PATH))
    {
        printf("ERROR: Failed to get executable path\n");
        return NULL;
    }
#else
    uint32_t moduleLen;
    char buffer[PATH_MAX];
    if (realpath("/proc/self/exe", buffer) == NULL)
    {
        printf("ERROR: Failed to get executable path\n");
        return NULL;
    }

    moduleLen = strlen(buffer);
#endif

    /* Path will be something like: C:\inflatelib\build\win\clang64release\tests\perf\perftests.exe
     *                     We want: C:\inflatelib\build\win\clang64release\tests\
     * So we can append 'filename'. We therefore need to find the position after the second to last '\' */
    for (int slashesSeen = 0; moduleLen > 0; --moduleLen)
    {
        if (buffer[moduleLen - 1] == PATH_SEPARATOR_CHR)
        {
            ++slashesSeen;
            if (slashesSeen == 2)
            {
                testsPathLen = moduleLen;
                break;
            }
        }
    }

    if (testsPathLen == 0)
    {
        printf("ERROR: Failed to find path to the 'data' directory\n");
        return NULL;
    }

    /* +5 because we also append "data\" */
    result = (char*)malloc(testsPathLen + 5 + filenameLen + 1);
    if (!result)
    {
        printf("ERROR: Failed to allocate space for path to file '%s'\n", filename);
        return NULL;
    }

    memcpy(result, buffer, testsPathLen);
    memcpy(result + testsPathLen, "data" PATH_SEPARATOR_STR, 5);
    memcpy(result + testsPathLen + 5, filename, filenameLen);
    result[testsPathLen + 5 + filenameLen] = '\0';

    return result;
}

file_data read_file(const char* filename)
{
    file_data result = {0};
    char* fullPath = NULL;
    FILE* file = NULL;
    uint8_t* buffer = NULL;
    uint8_t* writeBuffer = NULL;
    long fileSize = 0;
    size_t bytesRemaining = 0;

    fullPath = resolve_test_file_path(filename);
    if (!fullPath)
    {
        return result; /* Error already displayed */
    }

#ifdef _WIN32
    if (fopen_s(&file, fullPath, "rb"))
    {
        file = NULL;
    }
#else
    file = fopen(fullPath, "rb");
#endif

    if (!file)
    {
        printf("ERROR: Failed to open file '%s'\n", filename);
        printf("NOTE: Full path is '%s'\n", fullPath);
        free(fullPath);
        return result;
    }

    /* Determine the size of the file */
    if (fseek(file, 0, SEEK_END) != 0)
    {
        printf("ERROR: Failed to seek to end of file '%s'\n", filename);
        fclose(file);
        free(fullPath);
        return result;
    }

    fileSize = ftell(file);
    if (fileSize < 0)
    {
        printf("ERROR: Failed to get file size for '%s'\n", filename);
        fclose(file);
        free(fullPath);
        return result;
    }

    /* Seek back to the start */
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        printf("ERROR: Failed to seek to start of file '%s'\n", filename);
        fclose(file);
        free(fullPath);
        return result;
    }

    /* NOTE: We allocate one additional byte so that our last call to 'fread' will have enough space in the buffer to
     * attempt reading _something_ and will successfully hit/set EOF */
    buffer = (uint8_t*)malloc(fileSize + 1);
    if (!buffer)
    {
        printf("ERROR: Failed to allocate buffer of size %ld for file '%s'\n", fileSize, filename);
        fclose(file);
        free(fullPath);
        return result;
    }

    writeBuffer = buffer;
    bytesRemaining = (size_t)fileSize;
    while (1)
    {
        size_t bytesRead = fread(writeBuffer, 1, bytesRemaining + 1, file);
        if (bytesRead == 0)
        {
            /* There are three possible reasons for this:
             *  1.  We hit EOF
             *  2.  Buffer is full (unexpected change in file size)
             *  3.  We encountered an error
             */
            if (feof(file)) /* Case 1: EOF */
            {
                /* This doesn't necessarily mean success; it's possible the file got smaller (similar to case 2)*/
                if (bytesRemaining != 0)
                {
                    printf("ERROR: File '%s' changed size\n", filename);
                    printf("NOTE: Original size was %ld bytes; file became smaller\n", fileSize);
                    free(buffer);
                }
                else if (ferror(file))
                {
                    /* We hit an error */
                    printf("ERROR: Failed to read data from file '%s'\n", filename);
                    free(buffer);
                }
                else
                {
                    /* This is the success case */
                    result.filename = filename;
                    result.buffer = buffer;
                    result.bytes = fileSize;
                }
            }
            else if (bytesRemaining == 0) /* Case 2: Not EOF, but we've read what we thought was all the data */
            {
                printf("ERROR: File '%s' changed size\n", filename);
                printf("NOTE: Original size was %ld bytes; file became larger\n", fileSize);
                free(buffer);
            }
            else /* Case 3: We hit an error */
            {
                assert(ferror(file));
                printf("ERROR: Failed to read data from file '%s'\n", filename);
                free(buffer);
            }

            fclose(file);
            free(fullPath);
            return result;
        }

        assert(bytesRead <= bytesRemaining);
        writeBuffer += bytesRead;
        bytesRemaining -= bytesRead;
    }
}
