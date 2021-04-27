#include "simple_stopwatch.hpp"

#if __cplusplus < 201103L
// see https://svn.boost.org/trac10/ticket/13599
#    define BOOST_THREAD_HAS_CONDATTR_SET_CLOCK_MONOTONIC
#    define BOOST_THREAD_USES_CHRONO

#    include <boost/thread.hpp>
using namespace boost;
#else
// see https://en.cppreference.com/w/cpp/thread/condition_variable/wait_for
#    include <chrono>
#    include <condition_variable>
using namespace std;
#endif

#include <iostream>

#include <signal.h>
#include <unistd.h>

void handler(int signal)
{
    switch (signal) {
    case SIGALRM: // ignored
        break;
    }
}

volatile bool flag = {false};
bool predicate() {return flag == true;}

int main(int /*argc*/, char* /*argv*/[])
{
    signal(SIGALRM, &handler);
    ::alarm(1); // s

    condition_variable cv;
    mutex m;
    unique_lock<mutex> lock(m);
    {
        StopwatchReporter sw;

        if(cv.wait_for(lock, chrono::seconds(2), &predicate)) {
            std::cerr << "ERROR: timeout expected after 2s!" << std::endl;
            return EXIT_FAILURE;
        }
    }
    std::cout << "wait_for has returned with timout" << std::endl;

    return EXIT_SUCCESS;
}
