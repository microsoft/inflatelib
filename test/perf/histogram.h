/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include <stdint.h>

/* #define HISTOGRAM_COUNT 1000000 */

typedef struct histogram
{
    uint64_t* counts;
    size_t size;
    size_t capacity;

    /* Values updated as data is pushed */
    uint64_t min;
    uint64_t max;

    /* Data only valid after finalizing */
    double mean;
    double median; /* TODO: Maybe just round and save uint64_t? */
} histogram;

typedef struct histogram_buckets
{
    size_t* counts;
    size_t size;
    uint64_t start;
    uint64_t stride;
} histogram_buckets;

/* Returns 1 on success, 0 on failure */
int histogram_init(histogram* self, size_t capacity);

void histogram_destroy(histogram* self);

/* Returns 1 on success, 0 on failure */
int histogram_push(histogram* self, uint64_t value);

/* Called when all data has been added to the histogram */
void histogram_finalize(histogram* self);

/* All of the remaining functions can only be called after 'finalize' has been called */

histogram_buckets histogram_bucketize(histogram* self, uint64_t start, uint64_t stride, size_t bucketCount);

void histogram_destroy_buckets(histogram_buckets* self);

#endif
