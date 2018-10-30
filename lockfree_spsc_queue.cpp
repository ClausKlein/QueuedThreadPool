#include <boost/atomic.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/thread/thread.hpp>

#include <cassert>
#include <iostream>

// XXX #define NO_MAIN
// XXX #include "timeval_demo.cpp"
#include "simple_stopwatch.hpp"

const size_t iterations = 10000000;

typedef int data_type;

int producer_count(0);
int consumer_count(0);

boost::lockfree::spsc_queue<data_type, boost::lockfree::capacity<1024> >
    spsc_queue;
boost::atomic<bool> done(false);

typedef boost::chrono::system_simple_stopwatch Stopwatch;


void producer(void)
{
    // XXX timeval_demo::time_meas_helper measurement(BOOST_CURRENT_FUNCTION);
    Stopwatch timer;

    for (size_t i = 0; i != iterations; ++i) {
        data_type value = ++producer_count;
        while (!spsc_queue.push(value)) {
            boost::this_thread::yield();
        }
    }

    //FIXME std::cout << BOOST_CURRENT_FUNCTION << duration_cast<microseconds>(timer.elapsed()).count() << " us" << std::endl;
}


void consumer(void)
{
    // XXX timeval_demo::time_meas_helper measurement(BOOST_CURRENT_FUNCTION);
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

    //FIXME std::cout << BOOST_CURRENT_FUNCTION << duration_cast<microseconds>(timer.elapsed()).count() << " us" << std::endl;
}


int main(int, char**)
{
    using namespace boost::chrono;

    std::cout << "boost::lockfree::queue is ";
    if (!spsc_queue.is_lock_free()) {
        std::cout << "not ";
    }
    std::cout << "lockfree" << std::endl;

    {
        // XXX timeval_demo::time_meas_helper
        // measurement(BOOST_CURRENT_FUNCTION);
        Stopwatch timer;

        boost::thread producer_thread(producer);
        boost::thread consumer_thread(consumer);

        producer_thread.join();
        done = true;
        consumer_thread.join();

        std::cout << "elapsed time: " << timer.elapsed().count() << " us" << std::endl;
    }

    std::cout << "produced " << producer_count << " objects." << std::endl;
    std::cout << "consumed " << consumer_count << " objects." << std::endl;

    assert(producer_count == consumer_count);

    return 0;
}
