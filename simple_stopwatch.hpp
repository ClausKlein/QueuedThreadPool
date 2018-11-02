//  simple_stopwatch.hpp
//  ------------------------------------------------------------

#ifndef SIMPLE_STOPWATCH__HPP
#define SIMPLE_STOPWATCH__HPP

#define BOOST_CHRONO_VERSION 2

#include <boost/chrono/chrono_io.hpp>
#include <boost/chrono/stopwatches/reporters/stopwatch_reporter.hpp>
#include <boost/chrono/stopwatches/reporters/system_default_formatter.hpp>
#include <boost/chrono/stopwatches/simple_stopwatch.hpp>


namespace
{
////////////////////////////////////////////////////////////////////////////////////////////////
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
}

typedef boost::chrono::simple_stopwatch<> Stopwatch;
typedef boost::chrono::stopwatch_reporter<Stopwatch> StopwatchReporter;

#endif
