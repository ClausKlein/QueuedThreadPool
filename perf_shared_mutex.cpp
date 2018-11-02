//  (C) Copyright 2013 Andrey
//  (C) Copyright 2013 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// This performance test is based on the performance test provided by
// maxim.yegorushkin at https://svn.boost.org/trac/boost/ticket/7422

#include "simple_stopwatch.hpp"

#define BOOST_THREAD_USES_CHRONO
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread_only.hpp>

#include <iostream>

using namespace boost;

shared_mutex mtx;
const int cycles = 1000;

void shared()
{
    int cycle(0);
    while (++cycle < cycles) {
        shared_lock<shared_mutex> lock(mtx);
    }
}

void unique()
{
    int cycle(0);
    while (++cycle < cycles) {
        unique_lock<shared_mutex> lock(mtx);
    }
}

int main()
{
    chrono::high_resolution_clock::duration best_time(
        std::numeric_limits<chrono::high_resolution_clock::duration::rep>::max
            BOOST_PREVENT_MACRO_SUBSTITUTION());

    for (int i = 100; i > 0; --i) {
        Stopwatch timer;

        thread t0(shared);
        thread t1(shared);
        thread t2(unique);
        // thread t11(shared);
        // thread t12(shared);
        // thread t13(shared);

        t0.join();
        t1.join();
        t2.join();
        // t11.join();
        // t12.join();
        // t13.join();

        chrono::high_resolution_clock::duration elapsed(timer.elapsed());
        // std::cout << "     Time spent: "
        //           << duration_fmt(duration_style::symbol) << elapsed
        //           << std::endl;
        best_time =
            std::min BOOST_PREVENT_MACRO_SUBSTITUTION(best_time, elapsed);
    }

    std::cout << "Best Time spent: " << best_time << std::endl;
    std::cout << "Time spent/cycle:" << best_time / cycles / 3 << std::endl;

    return 0;
}
