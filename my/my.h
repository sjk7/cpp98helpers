// my.h
#pragma once
#ifndef MY_H_SJK_INCLUDED
#define MY_H_SJK_INCLUDED

#ifdef FORCE_VC6_COMPILER
#undef _MSC_VER
#define _MSC_VER 1200
#endif

#if (_MSC_VER > 1899)
#define MY_NO_EXCEPT noexcept

#else

#define MY_NO_EXCEPT
#define noexcept

#ifdef _MSC_VER
#pragma warning(disable : 4482) // non-standard enum use (qualified)
#pragma warning(disable : 4355) // 'this' used in base member initializer list
#endif
#endif

#ifdef _MSC_VER
#if (_MSC_VER <= 1200)
#define MSVC6

#ifndef constexpr
#define constexpr
#endif

#ifndef LONG
#define LONG long
#endif
#endif
#endif

#ifdef __MINGW32_MAJOR_VERSION
#if (__cplusplus <= 199711L)
#define constexpr
#endif
#endif

#if _MSC_VER <= 1200
#define constexpr
#include <stdlib.h>

#include <climits>

template <typename T> T swap_endian(T u) {
    // static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

static __forceinline unsigned long _byteswap_ulong(unsigned long val) {
    return swap_endian(val);
}

#else
#include <intrin.h>
#endif

#ifdef STEVE_SOURCE
#ifndef MY_H_SJK_SHOULD_INCLUDE_STDAFX
#define MY_H_SJK_SHOULD_INCLUDE_STDAFX
#endif

#ifdef MY_H_SJK_SHOULD_INCLUDE_STDAFX
#ifndef STEVE_SOURCE
#include <StdAfx.h>
#else

#endif
#endif
#endif

#include <string>

namespace my {

template <typename T>
static inline T increment_wrap(T value, T increment, T size) {
    T new_pos = value + increment;
    return (new_pos >= size) ? new_pos - size : new_pos;
}

template <typename T> T cppmin(const T a, const T b) {
    return (b < a) ? b : a;
}

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
    static const size_t BITS_IN_BYTE = 8;

    template <size_t POW2NUM> static constexpr inline bool is_power_of_2() {
        return (POW2NUM != 0) && ((POW2NUM & (POW2NUM - 1)) == 0);
    }
    template <typename T, size_t POW2NUM>
    inline const T modulo_power_of_2(T n) {
#if _MSC_VER > 1899
        static_assert(is_power_of_2<POW2NUM>(),
            "modulo_power_of_2: number must be a power of 2.");
#endif
        return (n & (POW2NUM - 1));
    }

    namespace binary {
        static __forceinline int BinaryToDecimal(const BYTE* pbinarybytes,
            const DWORD dwinbuflen, const DWORD startindex = 0,
            DWORD numbits = 4) {
            if (!pbinarybytes) return -1;

            int retval = 0;
            int endpos = dwinbuflen;
            if (numbits) endpos = numbits + startindex;

            if (endpos > (int)dwinbuflen) return -1;

            if (!numbits) numbits = endpos;
            assert(numbits <= 32);

            int decimal = 0;
            for (int i = startindex; i < endpos; i++) {
                int bin = int(pbinarybytes[i]);
                decimal = (decimal * 2) + bin;
            }
            return decimal;
        }

        // for (int i = BITS_IN_BYTE - 1; i > 0; i--) {
        // m_arbin[i] = (int)pow(2, i);
        namespace impl {
            static BYTE pow2_array[BITS_IN_BYTE]
                = {0, 2, 4, 8, 16, 32, 64, 128};
        }
        // why a string return value? SSO!
        static __forceinline BYTE* BytesToBits(BYTE output[256],
            const BYTE* pbytebuf, DWORD dwcbin, DWORD& dwbits) {

            int bytepos = 0;
            int bitpos = 0;
            int weight = 0;
            int bytesleft = (int)dwcbin;

            while (bytesleft > 0) {
                // memset(&output[bitpos], 0, BITS_IN_BYTE * sizeof(BYTE));
                output[bitpos] = 0;
                int bytevalue = pbytebuf[bytepos];

                if (bytevalue != 0) {

                    for (int i = 7; i >= 0; i--) {

                        weight = impl::pow2_array[i];

                        if (bytevalue >= impl::pow2_array[i] && bytevalue) {
                            output[bitpos] = 1;
                            bytevalue -= weight;
                        } else {
                            output[bitpos] = 0;
                        }
                        bitpos++;
                    }

                    bytesleft--;
                    bytepos++;
                } else {
                    bytesleft--;
                    bitpos += BITS_IN_BYTE;
                    bytepos++;
                }
            }

            return output;
        }

#ifndef _MSC_VER
        // big endian extract
        static inline unsigned long extract_i4(const unsigned char* const buf) {

            unsigned int x = 0;
            x = buf[0];
            x <<= 8;
            x |= buf[1];
            x <<= 8;
            x |= buf[2];
            x <<= 8;
            x |= buf[3];

            return x;
        }
#else

        static inline unsigned long extract_i4(const unsigned char* const buf) {
            return _byteswap_ulong(*(unsigned long*)(buf));
        }
#endif

    } // namespace binary
} // namespace numbers

} // namespace my

#endif
