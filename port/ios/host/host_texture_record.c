/* Apache-2.0. The menus' pictures from the upscaler's records: the guest asks for the original texture's record by its
key (level-0 hash and size, texture_record.h), and gets level 0 decoded to RGBA8. */
#include "host_texture_upscale.h"
#include "texture_record.h"
#include "texture_upscale_encode.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXIMUM_RECORD_BYTES (256u << 20)

/* 0: no intact record for the texture; else the record's level 0 size, width in the high 32 bits and height in the
   low, with rgba filled (width * height * 4 bytes, which capacity must hold) when rgba is not NULL */
unsigned long long host_texture_record_rgba(unsigned long long hash, unsigned int width, unsigned int height,
    void *rgba, unsigned int capacity)
{
    char directory[1024], path[1100];
    uint32_t out_width = 0, out_height = 0;
    unsigned char *file;
    long size;
    FILE *stream;
    int status;

    directory[0] = 0;
    host_texture_cache_directory(directory, sizeof(directory));
    if (!directory[0] || snprintf(path, sizeof(path), "%s/%016llx-%ux%u.bin", directory, hash, width, height) >= (int)sizeof(path))
        return 0;
    stream = fopen(path, "rb");
    if (!stream) return 0;
    if (fseek(stream, 0, SEEK_END) || (size = ftell(stream)) <= 0 || (unsigned long)size > MAXIMUM_RECORD_BYTES ||
        fseek(stream, 0, SEEK_SET))
    {
        fclose(stream);
        return 0;
    }
    if (!rgba)
    {
        /* (only the size is wanted: its header says, with no need to read and check the texels) */
        unsigned char header[TEXTURE_RECORD_FIXED_SIZE + 16 * TEXTURE_RECORD_MAXIMUM_LEVELS];
        size_t header_bytes = fread(header, 1, sizeof(header), stream);
        struct texture_record_layout layout;

        fclose(stream);
        return header_bytes >= TEXTURE_RECORD_FIXED_SIZE && !texture_record_check(header, header_bytes, (uint64_t)size, hash,
            width, height, texture_record_u32(header + 8), &layout) ? ((unsigned long long)layout.width << 32) | layout.height : 0;
    }
    file = malloc((size_t)size);
    if (!file || fread(file, 1, (size_t)size, stream) != (size_t)size)
    {
        fclose(stream);
        free(file);
        return 0;
    }
    fclose(stream);
    status = texture_record_decode_level0(file, (size_t)size, hash, width, height, rgba, capacity, &out_width, &out_height);
    free(file);
    return status == 1 ? ((unsigned long long)out_width << 32) | out_height : 0;
}
