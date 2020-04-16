// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

// SPSCAudio.cpp

#include <iostream>

#include "my_concurrent.h"
#include "my_spsc_buffer.h"



int main() {
#pragma comment(lib, "winmm.lib")
    timeBeginPeriod(1);
    test::test_spsc_buffer();

    {
        for (int i = 0; i < 1000; ++i) {
            test::test_threadex(i);
        }
    }

    {
        for (int i = 0; i < 5; ++i) {
            test::make_race(i);
        }
    }

    test::test_atomic();
    for (int i = 0; i < 40; ++i) {
        test::test_thread(i);
    }




    timeEndPeriod(1);
    return 0;
}
