/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#ifndef FILE_IO_H
#define FILE_IO_H

typedef struct file_data
{
    const char* filename; /* NOTE: Holds pointer to string literal; no need to free */
    uint8_t* buffer;
    size_t bytes;
} file_data;

/* Returns the number of bytes read. No file is empty, so zero means failure */
file_data read_file(const char* filename);

#endif
