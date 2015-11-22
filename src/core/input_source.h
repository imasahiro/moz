#include <stdio.h>
#include "memory.h"

#ifndef INPUT_SOURCE_H
#define INPUT_SOURCE_H

static unsigned char *load_file(const char *path, size_t *size, int align)
{
    size_t len;
    size_t readed;
    unsigned char *data;
    FILE *fp = fopen(path, "rb");
    assert(fp != 0);

    fseek(fp, 0, SEEK_END);
    len = (size_t) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = (unsigned char *) VM_CALLOC(1, len + 1 + align);
    readed = fread(data, 1, len, fp);
    assert(len == readed);
    fclose(fp);
    *size = len;
    return data;
    (void)readed;
}

#endif /* end of include guard */
