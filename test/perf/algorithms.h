/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <inflatelib.h>
#include <zlib.h>

#include "file_io.h"

/* The output buffer is selected as a constant size to emulate more real life scenarios */
static const size_t output_buffer_size = 1 << 16;

typedef enum deflate_algorithm
{
    deflate_algorithm_deflate = 0,
    deflate_algorithm_deflate64 = 1,
} deflate_algorithm;

const char* deflate_algorithm_string(deflate_algorithm alg);

typedef struct inflater_vtable
{
    int (*init)(void* pThis);
    void (*destroy)(void* pThis);
    const char* (*name)(void* pThis);
    int (*inflate_file)(void* pThis, const file_data* input, uint8_t* outputBuffer);
} inflater_vtable;

/* Convenient typedefs so these look more "object-like". The 'p' indicates that it's a "pointer to" an inflater */
typedef const inflater_vtable* const* pinflater;

typedef struct inflatelib_inflater_t
{
    const inflater_vtable* const vtable;
    inflatelib_stream stream;
} inflatelib_inflater_t;

extern inflatelib_inflater_t inflatelib_inflater;
extern inflatelib_inflater_t inflatelib_inflater64;

typedef struct zlib_inflater_t
{
    const inflater_vtable* const vtable;
    z_stream stream;
} zlib_inflater_t;

extern zlib_inflater_t zlib_inflater;

#endif
