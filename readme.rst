============================================
Thread pool and synchronization demo project
============================================

Unittest tracefile coverage info

+------------------------+------------+-----------+------------+
|                        |  Lines     | Functions | Branches   |
|                        |            |           |            |
|Filename                |Rate     Num|Rate    Num|Rate     Num|
+========================+============+===========+============+
|thread_pool.cpp         |78.4%     37|100%      6|34.5%    200|
+------------------------+------------+-----------+------------+
|threadpool.cpp          |97.4%    307|92.0%    50|58.8%    352|
+------------------------+------------+-----------+------------+
|threadpool.hpp          |100%      30|100%     18|61.1%     18|
+------------------------+------------+-----------+------------+
|threads_test.cpp        |97.4%    455|100%     55|49.6%   4028|
+------------------------+------------+-----------+------------+
|                  Total:|96.6%    829|96.9%   129|49.7%   4598|
+------------------------+------------+-----------+------------+


The code is still **-std=c++98** conform!

The test_suite run fine without errors ;-))


Notes
=====

There exist a generic lock algorithm

see https://en.cppreference.com/w/cpp/thread/lock

and https://en.cppreference.com/w/cpp/named_req/Lockable

Boost provides a version of this function that takes a sequence of *Lockable*
objects defined by a pair of iterators.  *std::scoped_lock* offers a **RAII**
wrapper for this function, and is generally preferred to a naked call to
*std::lock*.

    - https://www.boost.org/doc/libs/1_68_0/doc/html/thread/synchronization.html#thread.synchronization.lock_functions.lock_range


*std::condition_variable_any* can be used with *std::shared_lock* in order to
wait on a *std::shared_mutex* in shared ownership mode.  A possible use for
*std::condition_variable_any* with custom *Lockable* types is to provide
convenient interruptible waits: the custom lock operation would both lock the
associated mutex as expected, and also perform the necessary setup to notify
this condition variable when the interrupting signal is received.

    - https://en.cppreference.com/w/cpp/thread/condition_variable

    - https://en.cppreference.com/w/cpp/thread/condition_variable_any/wait


References
==========

    - A. Williams (2012), "C++ concurrency in action" 9.2.4 Interrupting a wait on *std::condition_variable_any*

    - https://en.cppreference.com/w/cpp/language/raii
