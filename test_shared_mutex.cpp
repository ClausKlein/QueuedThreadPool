#include "simple_stopwatch.hpp"

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#include <boost/thread.hpp>
#include <boost/thread/caller_context.hpp>
#include <boost/thread/detail/log.hpp>

boost::shared_mutex
    mtx; // warning: initialization of 'mutex' with static storage duration
         // may throw an exception that cannot be caught [cert-err58-cpp]
int g_cnt(5000000 / 10); // 5M/10 => 4005472571 nanoseconds

void writer()
{
    Stopwatch sw;
    for (;;) {
        boost::upgrade_lock<boost::shared_mutex> readlock(mtx);
        boost::upgrade_to_unique_lock<boost::shared_mutex> writelock(readlock);
        if (g_cnt > 0)
            --g_cnt;
        else
            break;
    }
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << sw.elapsed()
                     << BOOST_THREAD_END_LOG;
}

void reader()
{
    Stopwatch sw;
    for (;;) {
        boost::shared_lock<boost::shared_mutex> readlock(mtx);
        if (g_cnt <= 0)
            break;
    }
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << sw.elapsed()
                     << BOOST_THREAD_END_LOG;
}

void unique_locker()
{
    Stopwatch sw;
    for (;;) {
        boost::unique_lock<boost::shared_mutex> lock(mtx);
        if (g_cnt > 0)
            --g_cnt;
        else
            break;
    }
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << sw.elapsed()
                     << BOOST_THREAD_END_LOG;
}

int main()
{
    Stopwatch sw;
    boost::thread t0(writer);
    boost::thread t1(reader);
    boost::thread t2(unique_locker);

    t0.join();
    t1.join();
    t2.join();
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << sw.elapsed()
                     << BOOST_THREAD_END_LOG;

    return 0;
}
