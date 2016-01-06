#include "core/buffer.h"

void test_buffer_reader_read8()
{
    const char text[] = "hello";
    moz_buffer_reader_t R;
    moz_buffer_reader_init(&R, text, sizeof(text) - 1);
    assert(moz_buffer_reader_has_next(&R) == 1);
    assert(moz_buffer_reader_read8(&R) == 'h');
    assert(moz_buffer_reader_read8(&R) == 'e');
    assert(moz_buffer_reader_read8(&R) == 'l');
    assert(moz_buffer_reader_read8(&R) == 'l');
    assert(moz_buffer_reader_read8(&R) == 'o');
    assert(moz_buffer_reader_has_next(&R) == 0);
}

void test_buffer_reader_read_many()
{
    const char text[] = {0x1, 0x2, 0x3, 0x4};
    moz_buffer_reader_t R;
    moz_buffer_reader_init(&R, text, sizeof(text));
    assert(moz_buffer_reader_has_next(&R) == 1);
    assert(moz_buffer_reader_read16(&R) == (0x2 << 8 | 0x1));
    assert(moz_buffer_reader_read16(&R) == (0x4 << 8 | 0x3));
    assert(moz_buffer_reader_has_next(&R) == 0);
}

void test_buffer_writer_write8()
{
    moz_buffer_writer_t W;
    moz_buffer_reader_t R;

    moz_buffer_writer_init(&W, 32);
    assert(moz_buffer_writer_length(&W) == 0);
    moz_buffer_writer_write8(&W, 'h');
    moz_buffer_writer_write8(&W, 'e');
    moz_buffer_writer_write8(&W, 'l');
    moz_buffer_writer_write8(&W, 'l');
    moz_buffer_writer_write8(&W, 'o');
    assert(moz_buffer_writer_length(&W) == 5);

    moz_buffer_reader_init_from_writer(&R, &W);
    assert(moz_buffer_reader_read8(&R) == 'h');
    assert(moz_buffer_reader_read8(&R) == 'e');
    assert(moz_buffer_reader_read8(&R) == 'l');
    assert(moz_buffer_reader_read8(&R) == 'l');
    assert(moz_buffer_reader_read8(&R) == 'o');
    assert(moz_buffer_reader_has_next(&R) == 0);

    moz_buffer_writer_dispose(&W);
    assert(moz_buffer_writer_length(&W) == 0);
}

void test_buffer_writer_write_many()
{
    moz_buffer_writer_t W;
    moz_buffer_reader_t R;
    moz_buffer_writer_init(&W, 32);
    assert(moz_buffer_writer_length(&W) == 0);
    moz_buffer_writer_write32(&W, 0x40302010);
    moz_buffer_writer_write16(&W, (0x1 << 8 | 0x2));
    moz_buffer_writer_write16(&W, (0x3 << 8 | 0x4));
    assert(moz_buffer_writer_length(&W) == 8);

    moz_buffer_reader_init_from_writer(&R, &W);
    assert(moz_buffer_reader_has_next(&R) == 1);
    assert(moz_buffer_reader_read32(&R) == 0x40302010);
    assert(moz_buffer_reader_read16(&R) == (0x1 << 8 | 0x2));
    assert(moz_buffer_reader_read16(&R) == (0x3 << 8 | 0x4));
    assert(moz_buffer_reader_has_next(&R) == 0);

    moz_buffer_writer_dispose(&W);
    assert(moz_buffer_writer_length(&W) == 0);
}


int main(int argc, char const* argv[])
{
    test_buffer_reader_read8();
    test_buffer_reader_read_many();
    test_buffer_writer_write8();
    test_buffer_writer_write_many();
    return 0;
}
