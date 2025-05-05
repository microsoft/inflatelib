
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
