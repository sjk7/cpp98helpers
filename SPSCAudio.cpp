// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

// SPSCAudio.cpp

#include <iostream>
#include "my_concurrent.h"

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
template <typename T, size_t POW2NUM>
inline constexpr T modulo_power_of_2(T n, size_t pow2num) {
#if _MSC_VER > 1899
    static_assert(is_power_of_2<POW2NUM>(),
        "modulo_power_of_2: number must be a power of 2.");
#endif
    return (n & (POW2NUM - 1));
}
} // namespace numbers

template <size_t SIZE> struct spsc_data {
    spsc_data()
        : m_read_pos(0), m_write_pos(0), m_total_read(0), m_total_written(0) {}
    spsc_data(const spsc_data&);
    spsc_data& operator=(const spsc_data&);

    inline LONG read_pos() const {
        return concurrent::safe_read_value(m_read_pos);
    }
    inline LONG write_pos() const {
        return concurrent::safe_read_value(m_write_pos);
    }
    inline ULONGLONG total_read() const {
        return concurrent::safe_read_value(m_read_pos);
    }
    inline ULONGLONG total_written() const {
        return concurrent::safe_read_value(m_total_written);
    }

    // How many bytes can I read right now?
    inline LONG can_read() const {
        LONG ret = (LONG)(total_written() - total_read());
        assert(ret >= 0);
        return ret;
    }
    // How much free space?
    inline LONG can_write() const {
        LONG ret = (LONG)SIZE - (LONG)can_read();
        return ret;
    }
    // returs the *new* value set

    template <size_t BUFSIZE> inline LONG update_write_pos(LONG bytes_written) {
        LONG my_write_pos = bytes_written + m_write_pos;
        concurrent::safe_write_value(my_write_pos, m_write_pos);
        return my_write_pos;
    }

    inline ULONGLONG update_bytes_written(ULONGLONG bytes_written) {
        ULONGLONG new_bytes_written = m_total_written + bytes_written;
        concurrent::safe_write_value(new_bytes_written, m_total_written);
        return new_bytes_written;
    }

    inline ULONGLONG update_bytes_read(ULONGLONG bytes_read) {
        ULONGLONG new_bytes_read = m_total_read + bytes_read;
        concurrent::safe_write_value(new_bytes_read, m_total_read);
        return new_bytes_read;
    }

    private:
    volatile LONG m_read_pos;
    volatile LONG m_write_pos;
    volatile ULONGLONG m_total_read;
    volatile ULONGLONG m_total_written;
};

template <size_t SIZE> class spsc {
    public:
    typedef char byte_type;
    typedef spsc_data<SIZE> data_t;

    spsc() : m_buf(new byte_type[SIZE]) {
        memset(m_buf, 0, SIZE * sizeof(byte_type));
    };

    ~spsc() {
        delete[] m_buf;
        m_buf = 0;
    }

    inline const byte_type* const end() const { return m_buf + m_buf[SIZE]; }

    // return the actual number of bytes added to the buffer
    size_t write(const byte_type* const data, size_t lenb) {

        const size_t space = (size_t)m_data.can_write();
        const size_t ret = space < lenb ? space : lenb;
        if (ret) {
            size_t write_pos = (size_t)m_data.write_pos();
            byte_type* write_ptr = &m_buf[write_pos];
            const byte_type* const finish = write_ptr + write_pos;
            std::ptrdiff_t xtra = 0;
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
            m_data.update_write_pos<SIZE>((LONG)ret);
        }

        return ret;
    }

    private:
    byte_type* m_buf;
    spsc_data<numbers::mB> m_data;
};

class mythread : private concurrent::thread<mythread> {

    typedef concurrent::thread<mythread> base_t;
    friend class base_t;

    public:
    mythread(const char* id = 0) : base_t(id) {}
    int start() { return base_t::start(); }

    private:
    int on_thread() { return 0; }
};

int main() {
    // spsc<numbers::mB * 2> buf;
    // buf.write("Hello, world!", strlen("Hello, world!") + 1);
    // mythread thread("mythreadobj");
    // thread.start();
    concurrent::atomic_int<LONG> myint = 0;
    assert(myint == 0);
    myint = 77;
    assert(myint.get() == 77);
    assert(myint == 77);
    myint = myint + 1;
    assert(myint == 78);

    return 0;
}
