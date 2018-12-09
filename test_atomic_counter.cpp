
#include "simple_stopwatch.hpp"

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#include <boost/atomic.hpp>
#include <boost/thread/caller_context.hpp>
#include <boost/thread/detail/log.hpp>
#include <boost/thread/thread_only.hpp>

boost::atomic<int> g_cnt(5000000); // 5M => 220188364 nanoseconds

void writer()
{
    Stopwatch sw;
    for (;;) {
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
        if (g_cnt <= 0)
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
    boost::thread t2(writer);

    t0.join();
    t1.join();
    t2.join();
    BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << sw.elapsed()
                     << BOOST_THREAD_END_LOG;

    return 0;
}
