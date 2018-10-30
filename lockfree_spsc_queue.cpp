#include <boost/atomic.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/thread/thread.hpp>

#include <cassert>
#include <iostream>

#include "simple_stopwatch.hpp"

const size_t iterations = 10000000;

typedef int data_type;

int producer_count(0);
int consumer_count(0);

boost::lockfree::spsc_queue<data_type, boost::lockfree::capacity<1024> >
    spsc_queue;
boost::atomic<bool> done(false);

typedef boost::chrono::system_simple_stopwatch Stopwatch;

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

    std::cout << BOOST_CURRENT_FUNCTION
              << duration_cast<milliseconds>(timer.elapsed()).count() << " ms"
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

    std::cout << BOOST_CURRENT_FUNCTION
              << duration_cast<milliseconds>(timer.elapsed()).count() << " ms"
              << std::endl;
}


int main(int, char**)
{

    std::cout << "boost::lockfree::queue is ";
    if (!spsc_queue.is_lock_free()) {
        std::cout << "not ";
    }
    std::cout << "lockfree" << std::endl;

    Stopwatch timer;
    {
        boost::thread producer_thread(producer);
        boost::thread consumer_thread(consumer);

        producer_thread.join();
        done = true;
        consumer_thread.join();
    }
    std::cout << BOOST_CURRENT_FUNCTION
              << duration_cast<milliseconds>(timer.elapsed()).count() << " ms"
              << std::endl;

    std::cout << "produced " << producer_count << " objects." << std::endl;
    std::cout << "consumed " << consumer_count << " objects." << std::endl;

    assert(producer_count == consumer_count);

    return 0;
}
