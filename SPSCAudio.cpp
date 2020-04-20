// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

// SPSCAudio.cpp

#include <iostream>

#include "my_concurrent.h"
#define SPSC_TESTS_ENABLED
#include "my_spsc_buffer.h"

int main() {

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

    timeBeginPeriod(1);

    test::test_thread_stop_start();

    test::test_spsc_buffer_wrapping();
    test::test_spsc_buffer();

    {
        for (int i = 0; i < 15; ++i) {
            test::test_threadex(i);
        }
    }

    {
        for (int i = 0; i < 5; ++i) {
            test::make_race(i);
        }
    }

    test::test_atomic();
    for (int i = 0; i < 10; ++i) {
        test::test_thread(i);
    }

    timeEndPeriod(1);
    return 0;
}
