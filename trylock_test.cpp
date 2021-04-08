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

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_USES_CHRONO

#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_only.hpp>

#include <iostream>

#ifdef DEBUG
#    define VERBOSE
#endif

using namespace boost;

mutex mtx; // warning: initialization of 'mutex' with static storage duration
           // may throw an exception that cannot be caught [cert-err58-cpp]

int counter      = 0;
const int cycles = 1000;
const int worker = 2;


void monitor()
{
    do {
        boost::this_thread::sleep_for(ns(50));
        {
            unique_lock<mutex> lock(mtx, defer_lock);
            if (lock.try_lock()) {
                std::cout << counter << std::endl;

                if (counter >= (worker * cycles)) {
                    break;
                }
            }
        }
    } while (true);
}


void unique()
{
    int cycle(0);
    while (++cycle <= cycles) {
        unique_lock<mutex> lock(mtx);
        ++counter;
    }
}


int main()
{
    Stopwatch::duration best_time(std::numeric_limits<Stopwatch::rep>::max
            BOOST_PREVENT_MACRO_SUBSTITUTION());

    for (int i = 100; i > 0; --i) {
        Stopwatch timer;

        thread t1(unique);
        thread t2(unique);
        thread tm(monitor);

        t1.join();
        t2.join();
        tm.join();

        Stopwatch::duration elapsed(timer.elapsed());

#ifdef VERBOSE
        std::cout << "     Time spent: "
                  << chrono::duration_fmt(chrono::duration_style::symbol)
                  << elapsed << std::endl;
#endif

        best_time =
            std::min BOOST_PREVENT_MACRO_SUBSTITUTION(best_time, elapsed);
    }

    std::cout << "Best Time spent: " << best_time << std::endl;
    std::cout << "Time spent/cycle:" << best_time / cycles / worker
              << std::endl;

    return 0;
}
