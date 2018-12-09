// Copyright (C) 2012-2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include "simple_stopwatch.hpp"

#include <boost/config.hpp>

#if !defined BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_QUEUE_DEPRECATE_OLD

#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID

#include <boost/thread/detail/log.hpp>
#include <boost/thread/executor.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/thread_pool.hpp>

#include <exception>
#include <iostream>

#ifdef BOOST_MSVC
#pragma warning(disable : 4127) // conditional expression is constant
#endif


int runOutOfMemroy()
{
    BOOST_THREAD_LOG << " " << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
#if 0
    while (true) {
        new int[1000000000ul]; // throwing overload
    }
    return 0;
#else
    throw std::bad_alloc();
#endif
}

void p1()
{
    BOOST_THREAD_LOG << " " << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
}

void p2()
{
    BOOST_THREAD_LOG << " " << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
}

void submit_some(boost::basic_thread_pool& tp)
{
    tp.submit(&p1);
    tp.submit(&p2);
    tp.submit(&p1);
    tp.submit(&p2);
    tp.submit(&p1);
    tp.submit(&p2);
    tp.submit(&p1);
    tp.submit(&p2);
    tp.submit(&p1);
    tp.submit(&p2);
    boost::this_thread::sleep_for(ms(10));
}

int main()
{
    BOOST_THREAD_LOG << " " << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    {
        {
            StopwatchReporter sw;
            boost::basic_thread_pool tp(2);
            submit_some(tp);
        }
        try {
            StopwatchReporter sw;

#ifdef BOOST_THREAD_PROVIDES_EXECUTORS
            //==========================
            boost::executor_adaptor<boost::basic_thread_pool> ea(1);
            ea.submit(&p1);
            ea.submit(&p2);
            boost::future<int> t1 = boost::async(ea, &runOutOfMemroy);
            boost::this_thread::sleep_for(ms(10));
            std::cout << "\n t1 = " << t1.get() << std::endl;
            //==========================
#else
            runOutOfMemroy();
#endif

        } catch (std::exception& ex) {
            std::cerr << "\n ERRORRRRR " << ex.what() << std::endl;
            // return 1;
        } catch (...) {
            std::cerr << "\n ERRORRRRR exception thrown" << std::endl;
            // return 2;
        }
    }
    BOOST_THREAD_LOG << " " << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;

    return 0;
}
