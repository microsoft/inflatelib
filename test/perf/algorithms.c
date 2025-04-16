
#include "pch.h"

#include "algorithms.h"

const char* deflate_algorithm_string(deflate_algorithm alg)
{
    switch (alg)
    {
    case deflate_algorithm_deflate:
        return "Deflate";
    case deflate_algorithm_deflate64:
        return "Deflate64";
    default:
        assert(0);
        return "Unknown";
    }
}

static int inflatelib_inflater_init(void* pThis)
{
    inflatelib_inflater_t* self = (inflatelib_inflater_t*)pThis;
    if (inflatelib_init(&self->stream) != INFLATELIB_OK)
    {
        printf("ERROR: inflatelib_init failed\n");
        printf("ERROR: %s\n", self->stream.error_msg);
        return 0;
    }

    return 1;
}

const char* inflatelib_inflater_name(void* pThis)
{
    return "inflatelib";
}

static void inflatelib_inflater_destroy(void* pThis)
{
    inflatelib_inflater_t* self = (inflatelib_inflater_t*)pThis;
    inflatelib_destroy(&self->stream);
}

static int inflatelib_inflater_inflate(void* pThis, deflate_algorithm alg, const file_data* input, uint8_t* outputBuffer)
{
    inflatelib_inflater_t* self = (inflatelib_inflater_t*)pThis;
    int inflateResult;
    const int isDeflate = (alg == deflate_algorithm_deflate);

    inflatelib_reset(&self->stream);

    /* Initialize stream buffers */
    self->stream.next_in = input->buffer;
    self->stream.avail_in = input->bytes;

    while (1)
    {
        self->stream.next_out = outputBuffer;
        self->stream.avail_out = output_buffer_size;
        inflateResult = isDeflate ? inflatelib_inflate(&self->stream) : inflatelib_inflate64(&self->stream);
        if (inflateResult == INFLATELIB_EOF)
        {
            /* Tests for validity are done elsewhere; this is just a sanity check */
            assert(self->stream.avail_in == 0);
            return 1;
        }
        else if (inflateResult < 0)
        {
            /* Realistically should only happen because of bad data, which shouldn't ever be the case */
            assert(0);
            printf("ERROR: inflatelib_inflate%s unexpectedly failed for file '%s'\n", isDeflate ? "" : "64", input->filename);
            printf("ERROR: %s\n", self->stream.error_msg);
            return 0;
        }

        assert(self->stream.avail_in > 0);
    }
}

static const inflater_vtable inflatelib_inflater_vtable = {
    .init = inflatelib_inflater_init,
    .destroy = inflatelib_inflater_destroy,
    .name = inflatelib_inflater_name,
    .inflate_file = inflatelib_inflater_inflate,
};

inflatelib_inflater_t inflatelib_inflater = {
    .vtable = &inflatelib_inflater_vtable,
    .stream = {0},
};

int zlib_inflater_init(void* self)
{
    zlib_inflater_t* pThis = (zlib_inflater_t*)self;
    if (inflateInit2(&pThis->stream, -15) != Z_OK) /* Don't check for zlib header */
    {
        printf("ERROR: inflateInit2 failed\n");
        printf("ERROR: %s\n", pThis->stream.msg);
        return 0;
    }

    return 1;
}

void zlib_inflater_destroy(void* self)
{
    zlib_inflater_t* pThis = (zlib_inflater_t*)self;
    inflateEnd(&pThis->stream);
}

const char* zlib_inflater_name(void* pThis)
{
    return "zlib";
}

int zlib_inflater_inflate(void* self, deflate_algorithm alg, const file_data* input, uint8_t* outputBuffer)
{
    zlib_inflater_t* pThis = (zlib_inflater_t*)self;
    int inflateResult;

    assert(alg == deflate_algorithm_deflate);

    inflateReset(&pThis->stream);

    /* Initialize stream buffers */
    pThis->stream.next_in = input->buffer;
    pThis->stream.avail_in = input->bytes;

    while (1)
    {
        pThis->stream.next_out = outputBuffer;
        pThis->stream.avail_out = output_buffer_size;
        inflateResult = inflate(&pThis->stream, 0);
        if (inflateResult == Z_STREAM_END)
        {
            /* Tests for validity are done elsewhere; this is just a sanity check */
            assert(pThis->stream.avail_in == 0);
            return 1;
        }
        else if (inflateResult < 0)
        {
            /* Realistically should only happen because of bad data, which shouldn't ever be the case */
            assert(0);
            printf("ERROR: inflate unexpectedly failed for file '%s'\n", input->filename);
            printf("ERROR: %s\n", pThis->stream.msg);
            return 0;
        }

        assert(pThis->stream.avail_in > 0);
    }
}

static const inflater_vtable zlib_inflater_vtable = {
    .init = zlib_inflater_init,
    .destroy = zlib_inflater_destroy,
    .name = zlib_inflater_name,
    .inflate_file = zlib_inflater_inflate,
};

zlib_inflater_t zlib_inflater = {
    .vtable = &zlib_inflater_vtable,
    .stream = {0},
};
