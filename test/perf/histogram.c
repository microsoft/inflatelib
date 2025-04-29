
#include "pch.h"

#include "histogram.h"

int histogram_init(histogram* self, size_t capacity)
{
    memset(self, 0, sizeof(*self));

    self->counts = malloc(capacity * sizeof(*self->counts));
    if (!self->counts)
    {
        return 0; /* Failure */
    }

    self->capacity = capacity;

    /* min/max will be set to reasonable values on the first push */
    self->min = 0xFFFFFFFFFFFFFFFFull;

    return 1;
}

void histogram_destroy(histogram* self)
{
    if (self->counts)
    {
        free(self->counts);
    }

    memset(self, 0, sizeof(*self));
}

int histogram_push(histogram* self, uint64_t value)
{
    /* TODO: Could make this resize itself */
    if (self->size >= self->capacity)
    {
        return 0;
    }

    if (value < self->min)
    {
        self->min = value;
    }
    if (value > self->max)
    {
        self->max = value;
    }

    self->counts[self->size++] = value;
    return 1;
}

static int histogram_compare(const void* lhs, const void* rhs)
{
    uint64_t left = *(const uint64_t*)lhs;
    uint64_t right = *(const uint64_t*)rhs;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

void histogram_finalize(histogram* self)
{
    /* TODO: We use double to not worry about overflow, but may lose some precision; check to see if this is an issue */
    double sum = 0;
    size_t midpoint;

    /* First need to sort the data; this makes bucketizing a little easier and finding the median much easier */
    assert(self->size > 0);
    qsort(self->counts, self->size, sizeof(*self->counts), histogram_compare);

    /* Calculate the mean */
    for (size_t i = 0; i < self->size; ++i)
    {
        sum += self->counts[i];
    }
    self->mean = sum / (double)self->size;

    /* Calculate the median*/
    midpoint = self->size / 2;
    self->median = (double)self->counts[midpoint];
    if (self->size % 2 == 0)
    {
        self->median = (self->median + self->counts[midpoint - 1]) / 2;
    }
}

histogram_buckets histogram_bucketize(histogram* self, uint64_t start, uint64_t stride, size_t bucketCount)
{
    histogram_buckets result = {0};
    uint64_t next;
    size_t histIndex;

    result.counts = malloc(bucketCount * sizeof(*result.counts));
    if (!result.counts)
    {
        return result; /* Failure */
    }

    result.size = bucketCount;
    result.start = start;
    result.stride = stride;

    /* 'start' may be before our first value */
    histIndex = 0;
    while ((histIndex < self->size) && (self->counts[histIndex] < start))
    {
        ++histIndex;
    }

    next = start + stride;
    for (size_t i = 0; i < bucketCount; ++i)
    {
        size_t count = 0;
        while (histIndex < self->size)
        {
            if (self->counts[histIndex] < next)
            {
                ++count;
                ++histIndex;
            }
            else
            {
                break;
            }
        }

        result.counts[i] = count;
        next += stride;
    }

    return result;
}

void histogram_destroy_buckets(histogram_buckets* self)
{
    if (self->counts)
    {
        free(self->counts);
    }

    memset(self, 0, sizeof(*self));
}
