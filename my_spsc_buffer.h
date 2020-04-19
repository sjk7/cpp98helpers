#pragma once

#include "my_concurrent.h"

#include <algorithm>
// my_spsc_buffer.h
namespace numbers {
static const size_t kB = 1024;
static const size_t TWOkB = kB * 2;
static const size_t FOURkB = kB * 4;
static const size_t EIGHTkB = kB * 8;
static const size_t SIXTEENkB = kB * 16;
static const size_t THIRTY_TWOkB = kB * 32;
static const size_t SIXTY_FOURkB = kB * 64;
static const size_t HUNDRED_TWENTY_EIGHTkB = kB * 128;
static const size_t TWO_HUNDRED_FIFTY_SIXkB = kB * 256;
static const size_t FIVE_HUNDRED_TWELVEkB = kB * 512;
static const size_t mB = kB * 1024;

template <size_t POW2NUM> static constexpr inline bool is_power_of_2() {
    return (POW2NUM != 0) && ((POW2NUM & (POW2NUM - 1)) == 0);
}
template <typename T, size_t POW2NUM> inline const T modulo_power_of_2(T n) {
#if _MSC_VER > 1899
    static_assert(is_power_of_2<POW2NUM>(),
        "modulo_power_of_2: number must be a power of 2.");
#endif
    return (n & (POW2NUM - 1));
}

} // namespace numbers

namespace concurrent {
struct d {
    d()
        : read_pos(0)
        , write_pos(0)
        , total_read(0)
        , total_written(0)
        , end_of_data((ULONGLONG)-1) {}
    volatile mutable LONG read_pos;
    volatile mutable LONG write_pos;
    volatile mutable ULONGLONG total_read;
    volatile mutable ULONGLONG total_written;
    volatile mutable ULONGLONG end_of_data;
};

template <size_t SIZE> struct spsc_data {

    spsc_data() {
        assert(numbers::is_power_of_2<SIZE>()
            && "spsc_data: SIZE not a power of 2!");
    }
    NO_COPY_CLASS(spsc_data);

    inline size_t size() const { return SIZE; }
    inline LONG size_s() const { return (LONG)SIZE; }

    inline LONG read_pos() const {
        return concurrent::safe_read_value(m_d.read_pos);
    }
    inline LONG write_pos() const {
        return concurrent::safe_read_value(m_d.write_pos);
    }
    inline ULONGLONG total_read() const {
        return concurrent::safe_read_value(m_d.total_read);
    }

    inline ULONGLONG total_written() const {
        return concurrent::safe_read_value(m_d.total_written);
    }

    // How many bytes can I read right now?
    inline LONG can_read() const {
        const LONG w = (LONG)total_written();
        const LONG r = (LONG)total_read();
        LONG ret = w - r;
        assert(ret >= 0);
        return ret;
    }
    // How much free space?
    inline LONG can_write() const {
        LONG ret = (LONG)SIZE - (LONG)can_read();
        return ret;
    }

    // once you have a read a total of this number of bytes,
    // you have read it all:
    ULONGLONG data_end_count() const {
        return concurrent::safe_read_value(m_d.end_of_data);
    }

    void data_end_count_set(ULONGLONG byte_count) {
        concurrent::safe_write_value(m_d.end_of_data, byte_count);
    }

    // Although the individual values themselves are thread-safe,
    // of course setting them all is *not*. So be sure you are the
    // only one updating *any* of the values when you call clear().
    void clear() {
        concurrent::safe_write_value(m_d.total_written, (ULONGLONG)0);
        concurrent::safe_write_value(m_d.write_pos, (LONG)0);
        concurrent::safe_write_value(m_d.read_pos, (LONG)0);
        concurrent::safe_write_value(m_d.total_read, (ULONGLONG)0);
        concurrent::safe_write_value(m_d.end_of_data, (ULONGLONG)-1);
    }

    protected:
    // returns the *new* value set
    inline LONG update_write_pos(LONG bytes_written) {
        LONG my_write_pos = bytes_written + m_d.write_pos;
        my_write_pos = numbers::modulo_power_of_2<LONG, SIZE>(my_write_pos);
        assert(my_write_pos < (LONG)SIZE);
        concurrent::safe_write_value(m_d.write_pos, my_write_pos);
        assert(m_d.write_pos == my_write_pos);
        return my_write_pos;
    }

    inline LONG update_read_pos(LONG bytes_read) {
        LONG my_read_pos = bytes_read + m_d.read_pos;
        my_read_pos = numbers::modulo_power_of_2<LONG, SIZE>(my_read_pos);
        assert(my_read_pos < (LONG)SIZE);
        concurrent::safe_write_value(m_d.read_pos, my_read_pos);
        assert(m_d.read_pos == my_read_pos);
        return my_read_pos;
    }

    inline ULONGLONG update_bytes_written(ULONGLONG bytes_written) {
        ULONGLONG new_bytes_written = m_d.total_written + bytes_written;
        concurrent::safe_write_value(m_d.total_written, new_bytes_written);
        assert(m_d.total_written == new_bytes_written);
        return new_bytes_written;
    }

    inline ULONGLONG update_bytes_read(ULONGLONG bytes_read) {
        ULONGLONG new_bytes_read = m_d.total_read + bytes_read;
        concurrent::safe_write_value(m_d.total_read, new_bytes_read);
        assert(m_d.total_read == new_bytes_read);
        return new_bytes_read;
    }

    private:
    volatile d m_d;
};

template <size_t SIZE> class spsc_buffer : public spsc_data<SIZE> {

    public:
    typedef char byte_type;
    typedef spsc_data<SIZE> data_t;

    spsc_buffer() : m_buf(new byte_type[SIZE]) {
        memset(m_buf, 0, SIZE * sizeof(byte_type));
    };

    ~spsc_buffer() {
        delete[] m_buf;
        m_buf = 0;
    }

    inline const byte_type* begin() const { return m_buf; }
    inline const byte_type* end() const { return m_buf + SIZE; }

    size_t read(byte_type* target, size_t lenb) {
        using namespace std;
        assert(target && lenb);
#ifdef MSVC6
#define LONG ptrdiff_t;
        const size_t ret = std::_cpp_min(size_t(data_t::can_read()), lenb);
#else
        const size_t ret = (std::min)(size_t(data_t::can_read()), lenb);
#endif
        size_t my_read_pos = (size_t)data_t::read_pos();
        byte_type* read_ptr = &m_buf[my_read_pos];
        assert(my_read_pos < SIZE);
        byte_type* read_end = read_ptr + ret;
        size_t sz1 = ret;
        size_t sz2 = 0;
        if (read_end > end()) {
            ptrdiff_t xtra = read_end - end();
            sz1 -= xtra;
            assert(sz1 <= SIZE);
            sz2 = xtra;
        }
        memcpy(target, read_ptr, sz1);

        if (sz2) {

            target += sz1;
            memcpy(target, m_buf, sz2);
        }
        data_t::update_read_pos((LONG)ret);
        data_t::update_bytes_read(ret);

        return ret;
    }

    // return the actual number of bytes added to the buffer
    size_t write(const byte_type* const data, size_t lenb) {

        using namespace std;
        assert(lenb > 0 && "why would you want to write zero bytes?");
        const size_t space = (size_t)data_t::can_write();
        if (space == 0) {
            return 0;
        }
#ifdef MSVC6
#define LONG ptrdiff_t;
        const size_t ret = std::_cpp_min(space, lenb);
#else
        const size_t ret = (std::min)(space, lenb);
#endif

        if (ret) {

            size_t write_pos = (size_t)data_t::write_pos();

            byte_type* write_ptr = &m_buf[write_pos];
            const byte_type* const finish = write_ptr + ret;
            ptrdiff_t xtra = 0;
            size_t sz = ret;

            if (finish > end()) {
                xtra = finish - end();
                sz -= xtra;
                assert(sz <= lenb);
            }
            memcpy(write_ptr, data, sz);

            if (xtra) {
                memcpy(m_buf, data + sz, xtra);
            }

            data_t::update_write_pos(static_cast<long>(ret));

            data_t::update_bytes_written(static_cast<ULONGLONG>(ret));
        }

        return ret;
    }

    inline byte_type* buffer() { return m_buf; }

    private:
    byte_type* m_buf;
};

} // namespace concurrent

#ifdef SPSC_TESTS_ENABLED

template <size_t SIZE>
struct spsc_tester : public concurrent::spsc_buffer<SIZE> {
    typedef concurrent::spsc_buffer<SIZE> base;
};
namespace test {

void test_spsc_buffer() {

    concurrent::spsc_buffer<512> buf;
    assert(buf.size() == 512);
    assert(buf.size_s() == 512);
    int written = 0;

    for (int i = 0; i < 512; ++i) {
        size_t write = buf.write("x", 1);
        assert(write == 1);
        assert((int)buf.total_written() == i + 1);
        assert(buf.can_write() == (int)(buf.size() - (i + 1)));
        ++written;
    }

    size_t wrote = buf.write("y", 1);
    assert(wrote == 0); // coz its full
    assert(buf.can_write() == 0);
    assert(buf.can_read() == 512);
    assert(buf.total_read() == 0);
    assert(buf.total_written() == 512);

    buf.clear();
    assert(buf.can_write() == buf.size_s());
    assert(buf.can_read() == 0);
    assert(buf.total_read() == 0);
    assert(buf.total_written() == 0);
}

template <typename T>
bool has_consec_values(T* begin, T* end, T start_val = 0) {

    T expected = start_val;
    int ctr = 0;

    while (begin != end) {
        if (*begin++ != expected++) {
            return false;
        }
        ++ctr;
    }
    return true;
}

void test_spsc_buffer_wrapping() {
    spsc_tester<256> buf;
    static char cbuf[1000];
    char c = 0;
    // there is no std::iota() in c++98/0x
    for (size_t x = 0; x < 1000; ++x) {
        cbuf[x] = c++; // signed wrapping
    }

    assert(has_consec_values(cbuf, &cbuf[1000]));
    size_t wrote = buf.write(cbuf, 1000);
    assert(wrote == 256);
    assert(buf.write_pos() == 0);
    assert(buf.total_written() == 256);
    assert(buf.read_pos() == 0);
    assert(buf.total_read() == 0);
    assert(buf.can_read() == 256);

    assert(has_consec_values((char*)buf.begin(), (char*)buf.end()));
    char read_buf[1000];
    size_t read = buf.read(read_buf, 1000);
    assert(read == 256);
    assert(has_consec_values(read_buf, read_buf + 256));

    assert(buf.can_read() == 0);
    assert(buf.can_write() == buf.size_s());

    assert(buf.total_read() == 256);
    assert(buf.total_written() == 256);

    wrote = buf.write(cbuf, 255);
    assert(wrote == 255);
    wrote = buf.write(&cbuf[255], 10);
    assert(wrote == 1);

    read = buf.read(read_buf, 256);
    assert(has_consec_values(read_buf, read_buf + 256));

    buf.clear();
    assert(buf.can_read() == 0);
    assert(buf.can_write() == buf.size_s());
    assert(buf.total_read() == 0);
    assert(buf.total_written() == 0);
    assert(buf.read_pos() == 0);
    assert(buf.write_pos() == 0);

    // check wrap
    wrote = buf.write(cbuf, 240);
    assert(wrote == 240);
    read = buf.read(read_buf, 200);
    assert(read == 200);
    assert(has_consec_values(read_buf, read_buf + 200));
    assert(buf.can_read() == 40);

    wrote += buf.write(&cbuf[240], 100);
    assert(wrote == 340);
    assert(buf.can_read() == 100 + 40);

    read += buf.read(&read_buf[200], buf.can_read());
    assert(buf.can_read() == 0);
    ULONGLONG total_read = buf.total_read();
    assert((size_t)total_read == read);
    assert(has_consec_values(read_buf, read_buf + buf.total_read()));
    assert(buf.total_read() == read);
    assert(buf.total_written() == wrote);
}
} // namespace test
#endif // SPSC_TESTS_ENABLED

// namespace test
