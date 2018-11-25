============================================
Thread pool and synchronization demo project
============================================

Important: Note that most of this C++ code is still **-std=c++98** conform!
---------------------------------------------------------------------------


You only have to prevent the usage of new C++11 keywords like *auto*, *lambda*,
*initializer lists*, override, ...  Include **boost** header instead of **std**
header.

For examples have a look at my demo C++ test code.


Usage infos
-----------

Unittest tracefile coverage info
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

  TCOV=1 make
  # ...

+------------------------+------------+-----------+------------+
|                        |  Lines     | Functions | Branches   |
|                        |            |           |            |
|Filename                |Rate     Num|Rate    Num|Rate     Num|
+========================+============+===========+============+
|test_atomic_counter.cpp |100%      27|100%      4|45.1%    142|
+------------------------+------------+-----------+------------+
|thread_pool.cpp         |78.4%     37|100%      6|34.5%    200|
+------------------------+------------+-----------+------------+
|threadpool.cpp          |97.8%    314|92.0%    50|58.8%    352|
+------------------------+------------+-----------+------------+
|threadpool.hpp          |96,7%     30|100%     18|61.1%     18|
+------------------------+------------+-----------+------------+
|threads_test.cpp        |97.4%    455|100%     55|49.6%   4028|
+------------------------+------------+-----------+------------+
|                  Total:|97.6%    893|96.9%   129|49.7%   4598|
+------------------------+------------+-----------+------------+



And ctest run without errors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

::

  make ctest
  cd build && ctest -C debug
  Test project /Users/clausklein/Workspace/cpp/Threadpool/build
        Start  1: async_server
   1/24 Test  #1: async_server .....................   Passed    0.01 sec
        Start  2: chrono_io_ex1
   2/24 Test  #2: chrono_io_ex1 ....................   Passed    0.01 sec
        Start  3: daytime_client
   3/24 Test  #3: daytime_client ...................   Passed    0.01 sec
        Start  4: default_executor
   4/24 Test  #4: default_executor .................   Passed    1.01 sec
        Start  5: executor
   5/24 Test  #5: executor .........................   Passed    6.99 sec
        Start  6: lockfree_spsc_queue
   6/24 Test  #6: lockfree_spsc_queue ..............   Passed    1.26 sec
        Start  7: make_future
   7/24 Test  #7: make_future ......................   Passed    0.04 sec
        Start  8: multi_thread_pass
   8/24 Test  #8: multi_thread_pass ................   Passed    0.02 sec
        Start  9: perf_shared_mutex
   9/24 Test  #9: perf_shared_mutex ................   Passed    3.34 sec
        Start 10: serial_executor
  10/24 Test #10: serial_executor ..................   Passed    2.01 sec
        Start 11: shared_monitor
  11/24 Test #11: shared_monitor ...................   Passed    9.16 sec
        Start 12: shared_mutex
  12/24 Test #12: shared_mutex .....................   Passed   13.01 sec
        Start 13: shared_ptr
  13/24 Test #13: shared_ptr .......................   Passed    0.01 sec
        Start 14: stopwatch_reporter_example
  14/24 Test #14: stopwatch_reporter_example .......   Passed    0.01 sec
        Start 15: strict_lock
  15/24 Test #15: strict_lock ......................   Passed    0.01 sec
        Start 16: synchronized_person
  16/24 Test #16: synchronized_person ..............   Passed    0.01 sec
        Start 17: test_atomic_counter
  17/24 Test #17: test_atomic_counter ..............   Passed    0.62 sec
        Start 18: test_shared_mutex
  18/24 Test #18: test_shared_mutex ................   Passed    9.04 sec
        Start 19: thread_pool
  19/24 Test #19: thread_pool ......................   Passed    0.01 sec
        Start 20: thread_tss_test
  20/24 Test #20: thread_tss_test ..................   Passed    0.01 sec
        Start 21: trylock_test
  21/24 Test #21: trylock_test .....................   Passed    0.63 sec
        Start 22: user_scheduler
  22/24 Test #22: user_scheduler ...................   Passed    0.01 sec
        Start 23: volatile
  23/24 Test #23: volatile .........................   Passed    0.01 sec
        Start 24: test_threads
  24/24 Test #24: test_threads .....................   Passed    4.20 sec
  
  100% tests passed, 0 tests failed out of 24
  
  Total Test time (real) =  51.49 sec
  Claus-MBP:Threadpool clausklein$


C++14 Notes
===========

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

    - A. Williams (2012), **"C++ concurrency in action"** 9.2.4 Interrupting a wait on *std::condition_variable_any*

    - http://think-async.com/Asio/Documentation

    - https://en.cppreference.com/w/cpp/language/raii

    - https://en.cppreference.com/w/cpp/memory/enable_shared_from_this
