
#include "pch.h"

#include <inttypes.h>

#include <inflatelib.h>

#include "algorithms.h"
#include "histogram.h"

#ifdef _WIN32
#include <Windows.h>
#endif

/* Enough to get a reasonable amount of data */
static const size_t test_iterations = 1000;

/* The files we test for decoding */
static const char* const deflate_files[] = {
    "file.bin-write.deflate.exe.in.bin",
    "file.magna-carta.deflate.txt.in.bin",
    "file.us-constitution.deflate.txt.in.bin",
};

static const char* const deflate64_files[] = {
    "file.bin-write.deflate64.exe.in.bin",
    "file.magna-carta.deflate64.txt.in.bin",
    "file.us-constitution.deflate64.txt.in.bin",
};

const pinflater deflate_inflaters[] = {&inflatelib_inflater.vtable, &zlib_inflater.vtable};
const pinflater deflate64_inflaters[] = {&inflatelib_inflater64.vtable};

typedef struct test_desc
{
    /* Algorithm (Deflate or Deflate64) this is testing */
    deflate_algorithm algorithm;

    /* Input files to inflate & time */
    file_data* files;
    size_t file_count;

    /* The inflaters we are testing that support the chosen algorithm */
    const pinflater* inflaters;
    size_t inflater_count;

    /* There's one histogram per-inflater for overall runtime. There's also one histogram per file per inflater. All of
     * these historgrams are stored in a single array. The indexing is as follows:
     *      + For the per-inflater histograms: results[inflater_index]
     *      + For the per-file histograms: result[inflater_count + (file_index * file_count) + inflater_index]
     */
    histogram* results;
} test_desc;

static void test_desc_destroy(test_desc* self)
{
    size_t histogramCount = self->inflater_count + (self->inflater_count * self->file_count);

    if (self->files)
    {
        for (size_t i = 0; i < self->file_count; ++i)
        {
            free(self->files[i].buffer);
        }
        free(self->files);
    }

    if (self->results)
    {
        for (size_t i = 0; i < histogramCount; ++i)
        {
            histogram_destroy(&self->results[i]);
        }
        free(self->results);
    }

    for (size_t i = 0; i < self->inflater_count; ++i)
    {
        (*self->inflaters[i])->destroy((void*)self->inflaters[i]);
    }

    memset(self, 0, sizeof(*self));
}

static int test_desc_init(
    test_desc* self, deflate_algorithm alg, const char* const* fileNames, size_t fileCount, const pinflater* inflaters, size_t inflaterCount)
{
    size_t histogramCount = inflaterCount * (fileCount + 1);

    memset(self, 0, sizeof(*self));

    self->algorithm = alg;
    self->inflaters = inflaters;
    self->inflater_count = inflaterCount;

    /* Initialize all the inflaters*/
    for (size_t i = 0; i < inflaterCount; ++i)
    {
        if (!(*self->inflaters[i])->init((void*)self->inflaters[i]))
        {
            printf("ERROR: Failed to initialize inflater\n");
            test_desc_destroy(self);
            return 0;
        }
    }

    /* Load all the files */
    self->file_count = fileCount;
    self->files = (file_data*)malloc(fileCount * sizeof(file_data));
    if (!self->files)
    {
        printf("ERROR: Failed to allocate space for file data\n");
        test_desc_destroy(self);
        return 0;
    }

    for (size_t i = 0; i < fileCount; ++i)
    {
        self->files[i] = read_file(fileNames[i]);
        if (!self->files[i].buffer)
        {
            /* Error message already printed */
            test_desc_destroy(self);
            return 0;
        }
    }

    self->results = (histogram*)malloc(histogramCount * sizeof(*self->results));
    if (!self->results)
    {
        printf("ERROR: Failed to allocate space for histogram data\n");
        test_desc_destroy(self);
        return 0;
    }

    /* Initialize the histograms */
    for (size_t i = 0; i < histogramCount; ++i)
    {
        if (!histogram_init(&self->results[i], test_iterations))
        {
            printf("ERROR: Failed to initialize histogram\n");
            test_desc_destroy(self);
            return 0;
        }
    }

    return 1;
}

static histogram* test_desc_file_histogram(test_desc* self, size_t inflaterIndex, size_t fileIndex)
{
    return &self->results[self->inflater_count + (fileIndex * self->inflater_count) + inflaterIndex];
}

#ifdef _WIN32
static uint64_t current_time()
{
    LARGE_INTEGER result;
    BOOL success = QueryPerformanceCounter(&result);
    assert(success);

    return (uint64_t)result.QuadPart;
}

static double time_to_ms(uint64_t time)
{
    LARGE_INTEGER frequency;
    BOOL success = QueryPerformanceFrequency(&frequency);
    assert(success);

    return ((double)time / (double)frequency.QuadPart) * 1000.0;
}

static double time_to_ms_f(double time)
{
    LARGE_INTEGER frequency;
    BOOL success = QueryPerformanceFrequency(&frequency);
    assert(success);

    return (time / (double)frequency.QuadPart) * 1000.0;
}
#else

#endif

static int run_tests(test_desc* data);

int main()
{
    int result = 0;
    test_desc deflate_tests = {0};
    test_desc deflate64_tests = {0};

    if (!test_desc_init(
            &deflate_tests, deflate_algorithm_deflate, deflate_files, ARRAYSIZE(deflate_files), deflate_inflaters, ARRAYSIZE(deflate_inflaters)))
    {
        return 1;
    }

    if (!test_desc_init(
            &deflate64_tests, deflate_algorithm_deflate64, deflate64_files, ARRAYSIZE(deflate64_files), deflate64_inflaters, ARRAYSIZE(deflate64_inflaters)))
    {
        test_desc_destroy(&deflate_tests);
        return 1;
    }

    /* Finally, run the tests */
    if ((run_tests(&deflate_tests) != 0) || (run_tests(&deflate64_tests) != 0))
    {
        result = 1;
    }

    test_desc_destroy(&deflate_tests);
    test_desc_destroy(&deflate64_tests);
    return result;
}

int print_test_histogram(test_desc* tests, histogram* data, const char* title, size_t count, uint64_t width, uint64_t height);

static int run_tests(test_desc* data)
{
    /* The main purpose of this variable is to discourage the compiler from making some optimizations we would like to
     * avoid (such as dead writes, etc.)*/
    int result = 0;
    uint8_t* outputBuffer = NULL;
    uint64_t* times = NULL;

    printf("--------------------------------------------------------------------------------\n");
    printf("Running tests for %s...\n", deflate_algorithm_string(data->algorithm));

    outputBuffer = (uint8_t*)malloc(output_buffer_size);
    if (!outputBuffer)
    {
        printf("ERROR: Failed to allocate output buffer of size %zu\n", output_buffer_size);
        return 1;
    }

    /* We don't want to profile our histogram functions, so delay pushing each new value until we're done with all files */
    times = (uint64_t*)malloc(data->file_count * sizeof(*times));
    if (!times)
    {
        free(outputBuffer);
        printf("ERROR: Failed to allocate memory for timing data\n");
        return 1;
    }

    for (size_t iteration = 0; iteration < test_iterations; ++iteration)
    {
        if ((iteration % 100) == 0)
        {
            printf("Iteration %zu of %zu\n", iteration, test_iterations);
        }

        for (size_t inflaterIndex = 0; inflaterIndex < data->inflater_count; ++inflaterIndex)
        {
            uint64_t startTime = current_time(), totalTime;
            for (size_t fileCount = 0; fileCount < data->file_count; ++fileCount)
            {
                uint64_t fileStartTime = current_time();
                if (!(*data->inflaters[inflaterIndex])->inflate_file((void*)data->inflaters[inflaterIndex], &data->files[fileCount], outputBuffer))
                {
                    printf("ERROR: Failed to inflate file '%s'\n", data->files[fileCount].filename);
                    free(times);
                    free(outputBuffer);
                    return 1;
                }
                times[fileCount] = current_time() - fileStartTime;
            }
            totalTime = current_time() - startTime;

            /* Done with the tight-ish loop; we can now push all this data to the histograms */
            histogram_push(&data->results[inflaterIndex], totalTime);
            for (size_t fileCount = 0; fileCount < data->file_count; ++fileCount)
            {
                histogram_push(test_desc_file_histogram(data, inflaterIndex, fileCount), times[fileCount]);
            }
        }
    }

    /* All runs complete */
    for (size_t i = 0; i < (data->inflater_count * (data->file_count + 1)); ++i)
    {
        histogram_finalize(&data->results[i]);
    }

    /* TODO: Currently using a hard-coded width of 80... get the actual console size*/
    printf("\nSummary for %s:\n\n", deflate_algorithm_string(data->algorithm));
    if (!print_test_histogram(data, data->results, "Total Runtime", data->inflater_count, 80, 15))
    {
        result = 0;
    }
    else
    {
        for (size_t i = 0; i < data->file_count; ++i)
        {
            if (!print_test_histogram(data, test_desc_file_histogram(data, 0, i), data->files[i].filename, data->inflater_count, 80, 15))
            {
                result = 0;
                break;
            }
        }
    }

    free(times);
    free(outputBuffer);

    /* The odds of 'result' being zero is very low. On top of that, our results are deterministic; if we ever hit a
     * scenario where we get zero, we can just update its initial value to avoid this case */
    return (result != 0) ? 1 : 0;
}

/* TODO: Maybe just use colors? */
static const char histogram_symbols[] = {'#', '*', 'X', 'O'};

int print_test_histogram(test_desc* tests, histogram* data, const char* title, size_t count, uint64_t width, uint64_t height)
{
    size_t titleLen = strlen(title);
    uint64_t minX = 0xFFFFFFFFFFFFFFFFull, maxX = 0;
    uint64_t strideX, startX;
    uint64_t maxY = 0;
    uint64_t strideY;
    uint64_t extra;
    char* lastPrinted = NULL;
    histogram_buckets* buckets;
    double startXMs, strideXMs, labelDist = 0.01, totalXMs;

    assert(count <= ARRAYSIZE(histogram_symbols));

    lastPrinted = (char*)malloc(width * sizeof(*lastPrinted));
    if (!lastPrinted)
    {
        printf("ERROR: Failed to allocate memory for displaying summary\n");
        return 0;
    }
    memset(lastPrinted, ' ', width * sizeof(*lastPrinted));

    /* We want to try and avoid the biggest outliers, so we don't consider a set percentage of the highest and lowest
     * times when calculating the min & max. These values are heuristically chosen */
    const size_t outlierLowIndex = 0;
    const size_t outlierHighIndex = (test_iterations * 97) / 100;
    for (size_t i = 0; i < count; ++i)
    {
        uint64_t testMin = data[i].counts[outlierLowIndex];
        uint64_t testMax = data[i].counts[outlierHighIndex];

        if (testMin < minX)
        {
            minX = testMin;
        }
        if (testMax > maxX)
        {
            maxX = testMax;
        }
    }

    /* NOTE: This formula is really '((max - min + 1) + (width - 1)) / width' however the +/-1 cancel */
    strideX = (maxX - minX + width) / width;
    assert((width * strideX) > (maxX - minX));

    /* Try and center the data */
    extra = (width * strideX) - (maxX - minX + 1);
    startX = minX - (extra / 2);
    if (startX > minX)
    {
        startX = 0; /* Overflow */
    }
    assert(startX <= minX);
    assert((startX + width * strideX) > maxX);

    buckets = (histogram_buckets*)malloc(count * sizeof(*buckets));
    if (!buckets)
    {
        printf("ERROR: Failed to allocate memory for displaying summary\n");
        free(lastPrinted);
        return 0;
    }

    for (size_t i = 0; i < count; ++i)
    {
        buckets[i] = histogram_bucketize(data + i, startX, strideX, width);
        if (!buckets[i].counts)
        {
            printf("ERROR: Failed to allocate memory for displaying summary\n");
            for (size_t j = 0; j < i; ++j)
            {
                histogram_destroy_buckets(buckets + j);
            }
            free(buckets);
            free(lastPrinted);
            return 0;
        }
    }

    /* Now that we have the buckets set up, figure out the max Y value & calculate the vertical stride */
    for (size_t i = 0; i < count; ++i)
    {
        histogram_buckets* bucket = buckets + i;
        for (size_t j = 0; j < bucket->size; ++j)
        {
            if (bucket->counts[j] > maxY)
            {
                maxY = bucket->counts[j];
            }
        }
    }

    if (maxY < height)
    {
        /* Try to make the top of the graph not go too much over 100% */
        maxY = height;
    }

    strideY = (maxY + height - 1) / height;
    assert((strideY * height) >= maxY);

    /* Finally, print out the table & graph */

    /* Output the graph */
    printf("\n%*s\n", (int)(titleLen + (width + 9 - titleLen) / 2), title);
    for (uint64_t y = height; y > 0; --y)
    {
        uint64_t endY = y * strideY;
        uint64_t startY = endY - strideY;
        double startPctg = ((double)startY / (double)test_iterations) * 100.0;

        printf("%6.2f%% |", startPctg);

        for (uint64_t x = 0; x < width; ++x)
        {
            /* Figure out if a new bucket ended in this square */
            for (size_t i = 0; i < count; ++i)
            {
                if ((buckets[i].counts[x] > startY) && (buckets[i].counts[x] <= endY))
                {
                    lastPrinted[x] = histogram_symbols[i];
                }
            }

            printf("%c", lastPrinted[x]);
        }

        printf("\n");
    }

    printf("        +");
    for (uint64_t i = 0; i < width; ++i)
    {
        printf("-");
    }
    printf("\n");

    startXMs = time_to_ms(startX);
    strideXMs = time_to_ms(strideX);
    totalXMs = strideXMs * width;

    while ((totalXMs / labelDist) > 20)
    {
        labelDist *= 10;
    }

    printf("         ");
    for (uint64_t x = 0; x < width; ++x)
    {
        double start = startXMs + (x * strideXMs);
        double end = start + strideXMs;
        if ((uint64_t)(start / labelDist) != (uint64_t)(end / labelDist))
        {
            printf("|");

            /* See below for why this is done */
            for (int i = 0; (i < 5) && (++x < width); ++i)
            {
                printf(" ");
            }
        }
        else
        {
            printf(" ");
        }
    }
    printf("\n");

    printf("      ");
    for (uint64_t x = 0; x < width; ++x)
    {
        double start = startXMs + (x * strideXMs);
        double end = start + strideXMs;
        uint64_t base = end / labelDist;
        if (base != (uint64_t)(start / labelDist))
        {
            /* This prints out 5 characters, which is 4 more than we otherwise would and therefore need to advance our
             * counter by this extra amount. We add an additional one to ensure enough space for a ' ' */
            printf("%5.2f ", base * labelDist);
            x += 5;
        }
        else
        {
            printf(" ");
        }
    }
    printf("\n");

    /* Finally, the legend */
    printf("\nLegend:\n");
    for (size_t i = 0; i < count; ++i)
    {
        printf("  %c: %s\n", histogram_symbols[i], (*tests->inflaters[i])->name((void*)tests->inflaters[i]));
    }
    printf("\n");

    /* Output the table */
    printf("  Algorithm  | Minimum (ms) | Maximum (ms) | Average (ms) |  Median (ms)\n");
    printf("-------------+--------------+--------------+--------------+--------------\n");

    for (size_t i = 0; i < tests->inflater_count; ++i)
    {
        printf(
            "%12s | %12.5f | %12.5f | %12.5f | %12.5f\n",
            (*tests->inflaters[i])->name((void*)tests->inflaters[i]),
            time_to_ms(data[i].min),
            time_to_ms(data[i].max),
            time_to_ms_f(data[i].mean),
            time_to_ms_f(data[i].median));
    }
    printf("\n\n");

    /* Cleanup */
    for (size_t i = 0; i < count; ++i)
    {
        histogram_destroy_buckets(buckets + i);
    }
    free(buckets);

    free(lastPrinted);

    return 1;
}
