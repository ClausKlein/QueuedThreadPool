// Copyright (C) 2012-2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if !defined BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID

#include <boost/assert.hpp>
#include <boost/thread/caller_context.hpp>
#include <boost/thread/detail/log.hpp>
#include <boost/thread/executor.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/executor.hpp>
#include <boost/thread/executors/executor_adaptor.hpp>
#include <boost/thread/executors/inline_executor.hpp>
#include <boost/thread/executors/loop_executor.hpp>
#include <boost/thread/executors/serial_executor.hpp>
#include <boost/thread/executors/thread_executor.hpp>
#include <boost/thread/future.hpp>

#include <iostream>


#if defined BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION \
    && !defined BOOST_NO_CXX11_RVALUE_REFERENCES
static boost::future<void> p(boost::future<void> f)
{
    static boost::mutex mtx;
    boost::unique_lock<boost::mutex> l(mtx);

    std::cout << BOOST_CURRENT_FUNCTION << std::endl;
    BOOST_ASSERT(f.is_ready());
    return boost::make_ready_future();
}
#endif

static void p1()
{
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    boost::this_thread::sleep_for(boost::chrono::milliseconds(20));
}

static void p2()
{
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
}

static int f1()
{
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    boost::this_thread::sleep_for(boost::chrono::milliseconds(123));
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;

    return 1;
}

static int f2(int i)
{
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
    boost::this_thread::sleep_for(boost::chrono::milliseconds(345));
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;

    return i + 1;
}

void submit_some(boost::executor& tp)
{
    for (int i = 0; i < 3; ++i) {
        tp.submit(&p2);
    }
    for (int i = 0; i < 3; ++i) {
        tp.submit(&p1);
    }
}


void at_th_entry(boost::basic_thread_pool&)
{
    static boost::mutex mtx;
    boost::unique_lock<boost::mutex> l(mtx);
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << BOOST_THREAD_END_LOG;
}

int test_executor_adaptor()
{
    std::cout << BOOST_CONTEXTOF << std::endl;
    {
        try {
            std::cout << BOOST_CONTEXTOF << "default_executor" << std::endl;
            {
                boost::executor_adaptor<boost::basic_thread_pool> ea(4);
                std::cout << BOOST_CONTEXTOF << std::endl;
                submit_some(ea);
                std::cout << BOOST_CONTEXTOF << std::endl;

                {
                    boost::future<int> t1 = boost::async(ea, &f1);
                    boost::future<int> t2 = boost::async(ea, &f1);
                    std::cout << BOOST_CONTEXTOF << " t1 = " << t1.get()
                              << std::endl;
                    std::cout << BOOST_CONTEXTOF << " t2 = " << t2.get()
                              << std::endl;
                }

                std::cout << BOOST_CONTEXTOF << std::endl;
                submit_some(ea);
                std::cout << BOOST_CONTEXTOF << std::endl;
                {
                    boost::basic_thread_pool ea3(1);
                    std::cout << BOOST_CONTEXTOF << std::endl;
                    boost::future<int> t1 = boost::async(ea3, &f1);
                    std::cout << BOOST_CONTEXTOF << std::endl;
                    boost::future<int> t2 = boost::async(ea3, &f1);
                    std::cout << BOOST_CONTEXTOF << std::endl;
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
                    boost::future<int> t3 = boost::async(
                        ea3, f2, 1); // TODO This doesn't compiles yet on C++11
#else

                    boost::future<int> t3 =
                        boost::async(ea3, boost::bind(f2, 1));
#endif
                    std::cout << BOOST_CONTEXTOF << " t1 = " << t1.get()
                              << std::endl;
                    std::cout << BOOST_CONTEXTOF << " t2 = " << t2.get()
                              << std::endl;
                    std::cout << BOOST_CONTEXTOF << " t3 = " << t3.get()
                              << std::endl;
                }

                std::cout << BOOST_CONTEXTOF << std::endl;
                submit_some(ea);
                std::cout << BOOST_CONTEXTOF << std::endl;
                while (ea.try_executing_one()) {
                    boost::this_thread::yield();
                }
            }

#if 1
            std::cout << BOOST_CONTEXTOF << "loop_executor" << std::endl;
            {
                boost::executor_adaptor<boost::loop_executor> ea2;
                submit_some(ea2);
                ea2.underlying_executor().run_queued_closures();
                while (ea2.try_executing_one()) {
                    boost::this_thread::yield();
                }
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));
#endif

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
            std::cout << BOOST_CONTEXTOF << "serial_executor" << std::endl;
            {
                boost::executor_adaptor<boost::basic_thread_pool> ea1(4);
                boost::executor_adaptor<boost::serial_executor> ea2(ea1);
                submit_some(ea2);
                while (ea1.try_executing_one()) {
                    boost::this_thread::yield();
                }
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));
#endif

#if 1
            std::cout << BOOST_CONTEXTOF << "inline_executor" << std::endl;
            {
                boost::executor_adaptor<boost::inline_executor> ea1;
                submit_some(ea1);
                while (ea1.try_executing_one()) {
                    boost::this_thread::yield();
                }
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));
#endif

            std::cout << BOOST_CONTEXTOF << "thread_executor" << std::endl;
            {
                boost::executor_adaptor<boost::thread_executor> ea1;
                submit_some(ea1);
#if 0
                while (ea1.try_executing_one()) {
                    boost::this_thread::yield();
                }
#endif
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));

            std::cout << BOOST_CONTEXTOF
                      << "boost::basic_thread_pool(4, at_th_entry)"
                      << std::endl;
            {
                boost::basic_thread_pool ea(4, at_th_entry);
                boost::future<int> t1 = boost::async(ea, &f1);
                std::cout << BOOST_CONTEXTOF << " t1 = " << t1.get()
                          << std::endl;

#if 0
                while (ea.try_executing_one()) {
                    boost::this_thread::yield();
                }
#endif
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));

            std::cout << BOOST_CONTEXTOF << "boost::async" << std::endl;
            {
                boost::async(&f1);
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));

            std::cout << BOOST_CONTEXTOF << "basic_thread_pool and async"
                      << std::endl;
            {
                boost::basic_thread_pool ea(1);
                boost::async(ea, &f1);

#if 0
                while (ea.try_executing_one()) {
                    boost::this_thread::yield();
                }
#endif
            }
            boost::this_thread::sleep_for(boost::chrono::seconds(1));

            std::cout << BOOST_CONTEXTOF << std::endl;
            boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
            std::cout << BOOST_CONTEXTOF << std::endl;
        } catch (std::exception& ex) {
            std::cout << "ERROR = " << ex.what() << "" << std::endl;
            return 1;
        } catch (...) {
            std::cout << " ERROR = exception thrown" << std::endl;
            return 2;
        }
    }
    std::cout << BOOST_CONTEXTOF << std::endl;

    return 0;
}


int main()
{

#if defined BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION \
    && defined BOOST_THREAD_PROVIDES_EXECUTORS \
    && !defined BOOST_NO_CXX11_RVALUE_REFERENCES
    boost::basic_thread_pool executor;

    boost::make_ready_future().then(&p);
    boost::make_ready_future().then(executor, &p);
#endif

    return test_executor_adaptor();
}
