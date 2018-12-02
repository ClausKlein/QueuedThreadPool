// Copyright (C) 2012-2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID

#if !defined BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

// XXX #include <boost/thread.hpp>
#include <boost/thread/detail/log.hpp>
// XXX #include <boost/thread/executor.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/executor_adaptor.hpp>
#include <boost/thread/future.hpp>

#ifdef BOOST_MSVC
#pragma warning(disable : 4127) // conditional expression is constant
#endif

#include <exception>


int runOutOfMemroy()
{
#if 0
    while (true) {
        new int[100000000ul]; // throwing overload
    }
    return 0;
#else
    throw std::bad_alloc();
#endif
}

void p1()
{
    BOOST_THREAD_LOG << boost::this_thread::get_id() << " "
                     << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
}

void p2()
{
    BOOST_THREAD_LOG << boost::this_thread::get_id() << " "
                     << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
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
}

int main()
{
    BOOST_THREAD_LOG << boost::this_thread::get_id() << " "
                     << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    {
        boost::basic_thread_pool tp;
        try {
            submit_some(tp);
            //==========================
            // TODO: boost::executor_adaptor<boost::basic_thread_pool> ea;
            // TODO: boost::future<int> t1 = boost::async(ea, &runOutOfMemroy);
            runOutOfMemroy();
            //==========================
        } catch (std::exception& ex) {
            BOOST_THREAD_LOG << " ERRORRRRR " << ex.what()
                             << BOOST_THREAD_END_LOG;
            // return 1;
        } catch (...) {
            BOOST_THREAD_LOG << " ERRORRRRR exception thrown"
                             << BOOST_THREAD_END_LOG;
            // return 2;
        }
    }
    BOOST_THREAD_LOG << boost::this_thread::get_id() << " "
                     << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;

    return 0;
}
