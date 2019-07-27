//  simple_stopwatch.hpp
//  ------------------------------------------------------------

#ifndef SIMPLE_STOPWATCH__HPP
#define SIMPLE_STOPWATCH__HPP

#define BOOST_CHRONO_VERSION 2
#define BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING 1

#include <boost/chrono/chrono_io.hpp>

#include "boost/chrono/stopwatches/reporters/stopwatch_reporter.hpp"
#include "boost/chrono/stopwatches/reporters/system_default_formatter.hpp"
#include "boost/chrono/stopwatches/strict_stopwatch.hpp"
#include "boost/chrono/stopwatches/simple_stopwatch.hpp"

typedef boost::chrono::high_resolution_clock Clock;

namespace simple
{
////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: non portable! CK
// class Stopwatch {
// public:
//     typedef long long rep;
//
//     static rep now()
//     {
//         timespec ts;
//         if (clock_gettime(CLOCK_MONOTONIC, &ts)) abort();
//         return ts.tv_sec * rep(1000000000) + ts.tv_nsec;
//     }
//
//     Stopwatch()
//         : start_(now())
//     {}
//
//     rep elapsed() const { return now() - start_; }
//
// private:
//     rep start_;
// };
////////////////////////////////////////////////////////////////////////////////////////////////
template <class Clock> class simple_stopwatch {
    typename Clock::time_point start;

public:
    typedef long long rep;

    simple_stopwatch()
        : start(Clock::now())
    {}
    typename Clock::duration elapsed() const { return Clock::now() - start; }
    rep seconds() const
    {
        return elapsed().count()
            * (static_cast<rep>(Clock::period::num) / Clock::period::den);
    }
};

typedef simple_stopwatch<Clock> SimpleStopwatch;
}

typedef boost::chrono::strict_stopwatch<> StrictStopwatch;
typedef boost::chrono::simple_stopwatch<> Stopwatch;
typedef boost::chrono::stopwatch_reporter<Stopwatch> StopwatchReporter;

typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

#endif
