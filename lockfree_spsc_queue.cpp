#define BOOST_THREAD_VERSION 5
#include "simple_stopwatch.hpp"

#include <boost/assert.hpp>
#include <boost/atomic.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>

const size_t iterations = 10000000;

typedef int data_type;

int producer_count(0);
int consumer_count(0);

boost::lockfree::spsc_queue<data_type, boost::lockfree::capacity<1024> >
    spsc_queue; // warning: initialization of 'spsc_queue' with static storage
                // duration may throw an exception that cannot be caught
                // [cert-err58-cpp]
boost::atomic<bool> done(false);

using namespace boost::chrono;

void producer(void)
{
    Stopwatch timer;

    for (size_t i = 0; i != iterations; ++i) {
        data_type value = ++producer_count;
        while (!spsc_queue.push(value)) {
            boost::this_thread::yield();
        }
    }

    std::cout << BOOST_CURRENT_FUNCTION << ": "
              << duration_fmt(duration_style::symbol) << timer.elapsed()
              << std::endl;
}

void consumer(void)
{
    Stopwatch timer;

    data_type value = 0;
    while (!done) {
        while (spsc_queue.pop(value)) {
            ++consumer_count;
        }
        boost::this_thread::yield();
    }

    while (spsc_queue.pop(value)) {
        ++consumer_count;
    }

    std::cout << BOOST_CURRENT_FUNCTION << ": "
              << duration_fmt(duration_style::symbol) << timer.elapsed()
              << std::endl;
}

int main(int, char**)
{

    std::cout << "boost::lockfree::queue is ";

#if defined(__cplusplus) && (__cplusplus < 201103L) && !defined(__LINUX__)
    if (!spsc_queue.is_lock_free()) {
        std::cout << "not ";
    }
#endif

    std::cout << "lockfree" << std::endl;

    {
        Stopwatch timer;

        boost::thread producer_thread(producer);
        boost::thread consumer_thread(consumer);

        producer_thread.join();
        done = true;
        consumer_thread.join();

        std::cout << BOOST_CURRENT_FUNCTION << ": "
                  << duration_fmt(duration_style::symbol) << timer.elapsed()
                  << std::endl;
    }

    std::cout << "produced " << producer_count << " objects." << std::endl;
    std::cout << "consumed " << consumer_count << " objects." << std::endl;

    BOOST_ASSERT(producer_count == consumer_count);

    return 0;
}
