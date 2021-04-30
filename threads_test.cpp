//
// test program for agentpp threadpool
//
// astyle --style=kr thread*.{cpp,hpp}
// clang-format -style=file -i thread*.{cpp,hpp}
//

#ifdef USE_AGENTPP
#    include "agent_pp/threads.h" // ThreadPool, QueuedThreadPool
using namespace Agentpp;
#elif USE_AGENTPP_CK
#    include "posix/threadpool.hpp" // ThreadPool, QueuedThreadPool
#    define TEST_INDEPENDENTLY
#    define USE_BUSY_TEST
#    define TEST_USAGE_AFTER_TERMINATE
using namespace AgentppCK;
#else
#    include "threadpool.hpp" // ThreadPool, QueuedThreadPool
#    define USE_WAIT_FOR
#    define USE_BUSY_TEST
using namespace Agentpp;
#endif

#include "simple_stopwatch.hpp"

#ifndef _WIN32
// -----------------------------------------
#    define BOOST_TEST_MODULE Threads
#    define BOOST_TEST_NO_MAIN
#    include <boost/test/included/unit_test.hpp>
// -----------------------------------------
#else
#    include <boost/test/auto_unit_test.hpp>
#endif

#include <boost/atomic.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/latch.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_only.hpp>

#include <iostream>
#include <string>
#include <vector>

#if !defined BOOST_THREAD_TEST_TIME_MS
#    if defined(__linux__) || defined(__APPLE__)
#        define BOOST_THREAD_TEST_TIME_MS 75
#    else
// Windows, Cygwin, msys all need this
#        define BOOST_THREAD_TEST_TIME_MS 250
#    endif
#endif

typedef boost::atomic<size_t> test_counter_t;
typedef boost::lockfree::queue<size_t, boost::lockfree::capacity<20> >
    result_queue_t;

class TestTask : public Runnable {
    typedef boost::mutex lockable_type;
    typedef boost::unique_lock<lockable_type> scoped_lock;

public:
    explicit TestTask(
        const std::string& msg, result_queue_t& rslt, unsigned ms_delay = 11)
        : text(msg)
        , result(rslt)
        , delay(ms_delay)
    {
        scoped_lock l(lock);
        ++counter;
    }

    ~TestTask() BOOST_OVERRIDE
    {
        scoped_lock l(lock);
        --counter;
    }

#ifdef USE_UNIQUE_PTR
    boost::unique_ptr<Runnable> clone() const BOOST_OVERRIDE
    {
        return boost::make_unique<TestTask>(text, result, delay);
    }
#endif

    void run() BOOST_OVERRIDE
    {
        Thread::sleep((rand() % 3) * delay); // NOLINT

        scoped_lock l(lock);
        // WARNING: ThreadSanitizer: data race
        // BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << " called with: " <<
        // text);
        size_t hash = boost::hash_value(text);
        result.push(hash);
        ++run_cnt;
    }

    static size_t run_count()
    {
        scoped_lock l(lock);
        return run_cnt;
    }
    static size_t task_count()
    {
        scoped_lock l(lock);
        return counter;
    }
    static void reset_counter()
    {
        scoped_lock l(lock);
        counter = 0;
        run_cnt = 0;
    }

protected:
    static test_counter_t run_cnt;
    static test_counter_t counter;
    static lockable_type lock;

private:
    const std::string text;
    result_queue_t& result;
    const unsigned delay;
};

TestTask::lockable_type TestTask::lock;
//  warning: initialization of 'lock' with static storage
//  duration may throw an exception that cannot be caught
//  [cert-err58-cpp]

test_counter_t TestTask::run_cnt(0);
test_counter_t TestTask::counter(0);

static boost::latch start_latch(1);
static boost::latch completion_latch(4);

void push_task(ThreadPool* tp)
{
    static result_queue_t result;

    start_latch.wait();

    tp->execute(new TestTask(
        "Generate to mutch load.", result, BOOST_THREAD_TEST_TIME_MS));

    completion_latch.count_down();
}

#ifndef USE_AGENTPP
BOOST_AUTO_TEST_CASE(ThreadPoolInterface_test)
{
    result_queue_t result;
    ThreadPool emptyThreadPool(0);

#    ifdef USE_BUSY_TEST
    BOOST_TEST(emptyThreadPool.is_busy());
#    endif

    BOOST_TEST(!emptyThreadPool.is_idle());

    emptyThreadPool.execute(new TestTask("I want to run!", result));

    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 0UL, "NO task can to be executed!");

    TestTask::reset_counter();
    emptyThreadPool.terminate();
}
#endif

BOOST_AUTO_TEST_CASE(ThreadPool_busy_test)
{
    constexpr size_t MAX_LOAD { 8 };
    constexpr size_t threadCount { 1 };
    {
        constexpr size_t stacksize { AGENTPP_DEFAULT_STACKSIZE * 2 };
        ThreadPool threadPool(threadCount, stacksize);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == threadCount);
        BOOST_TEST(threadPool.stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());

        // call execute parallel from different task!
        start_latch.reset(1);
        completion_latch.reset(MAX_LOAD);
        std::array<boost::thread, (MAX_LOAD)> threads;
        for (size_t i = 0; i < (MAX_LOAD); ++i) {
            threads.at(i) = boost::thread(push_task, &threadPool);
            threads.at(i).detach();
        }
        start_latch.count_down();
        boost::this_thread::yield();
        completion_latch.wait();

#ifdef USE_BUSY_TEST
        // FIXME! CK BOOST_TEST(threadPool.is_busy());
#endif

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

#ifdef USE_BUSY_TEST
        BOOST_TEST(!threadPool.is_busy());
#endif

        threadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL);
    BOOST_TEST(TestTask::run_count() == (MAX_LOAD));
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(ThreadPool_test)
{
    result_queue_t result;
    size_t i = 0;
    {
        ThreadPool threadPool(4UL);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 4UL);
        BOOST_TEST_WARN(threadPool.stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(threadPool.is_idle());

#ifdef USE_BUSY_TEST
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("Hallo world!", result));
        ++i;
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("ThreadPool is running!", result));
        ++i;
        BOOST_TEST(!threadPool.is_busy());
#endif

        do {
            threadPool.execute(new TestTask("Generate some load.", result));
            ++i;
        } while (threadPool.is_idle());

        threadPool.execute(new TestTask("Under full load now!", result));
        ++i;
        threadPool.execute(new TestTask("Good by!", result));
        ++i;

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());

#ifdef USE_BUSY_TEST
        BOOST_TEST(!threadPool.is_busy());
#endif

        threadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == i, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_busy_test)
{
    result_queue_t result;
    constexpr size_t MAX_LOAD { 4 };
    constexpr size_t threadCount { MAX_LOAD / 2 };
    {
        constexpr size_t stacksize { AGENTPP_DEFAULT_STACKSIZE * 2 };
        QueuedThreadPool threadPool(threadCount, stacksize);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        threadPool.start(); // NOTE: different to ThreadPool, but this
                            // should not really needed!
#endif

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == threadCount);
        BOOST_TEST(threadPool.stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());

        // call execute parallel from different task!
        start_latch.reset(1);
        completion_latch.reset(MAX_LOAD);
        std::array<boost::thread, (MAX_LOAD)> threads;
        for (size_t i = 0; i < (MAX_LOAD); ++i) {
            threads.at(i) = boost::thread(push_task, &threadPool);
            threads.at(i).detach();
        }
        start_latch.count_down();
        boost::this_thread::yield();
        completion_latch.wait();

#ifdef USE_BUSY_TEST
        // FIXME! CK BOOST_TEST(threadPool.is_busy());
#endif

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

#ifdef USE_BUSY_TEST
        // FIXME! CK BOOST_TEST(!threadPool.is_busy());
#endif

        threadPool.terminate();
    }
    BOOST_TEST(TestTask::run_count() == (MAX_LOAD));
    BOOST_TEST(TestTask::task_count() == 0UL);
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_test)
{
    result_queue_t result;
    {
        QueuedThreadPool queuedThreadPool(1);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        queuedThreadPool.start(); // NOTE: different to ThreadPool, but this
                                  // should not really needed!
#endif

        BOOST_TEST_MESSAGE(
            "queuedThreadPool.size: " << queuedThreadPool.size());
        BOOST_TEST(queuedThreadPool.size() == 1UL);

        BOOST_TEST_WARN(
            queuedThreadPool.stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(queuedThreadPool.is_idle());

        queuedThreadPool.execute(new TestTask("1 Hi again.", result, 10));
        queuedThreadPool.execute(
            new TestTask("2 Queuing starts.", result, 20));
        queuedThreadPool.execute(
            new TestTask("3 Under full load!", result, 30));

        std::srand(static_cast<unsigned>(std::time(0)));
        unsigned i = 4;
        do {
            unsigned delay = rand() % 100; // NOLINT
            std::string msg(std::to_string(i));
            queuedThreadPool.execute(
                new TestTask(msg + " Queuing ...", result, delay));
        } while (++i < (4 + 6));

        do {
            BOOST_TEST_MESSAGE("queuedThreadPool.queue_length: "
                << queuedThreadPool.queue_length());
            Thread::sleep(500); // NOTE: after more than 1/2 sec! CK
        } while (!queuedThreadPool.is_idle());

#ifdef USE_BUSY_TEST
        BOOST_TEST(!queuedThreadPool.is_busy());
#endif

        queuedThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 9UL, "All task has to be executed!");
    TestTask::reset_counter();

    BOOST_TEST_MESSAGE("NOTE: checking the order of execution");
    for (size_t i = 1; i < 10; i++) {
        size_t value;
        if (result.pop(value)) {
            if (i >= 4) {
                std::string msg(
                    boost::lexical_cast<std::string>(i) + " Queuing ...");
                BOOST_TEST_WARN(
                    boost::hash_value(value) == boost::hash_value(msg),
                    "expected msg: " << msg);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolLoad_test)
{
    result_queue_t result;
    {
        QueuedThreadPool defaultThreadPool(1UL);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        defaultThreadPool.start(); // NOTE: different to ThreadPool, but this
                                   // should not really needed!
#endif

        BOOST_TEST(defaultThreadPool.is_idle());

        BOOST_TEST_MESSAGE(
            "defaultThreadPool.size: " << defaultThreadPool.size());
        defaultThreadPool.execute(new TestTask("Started ...", result));

        unsigned i = 20;
        do {
            if (i > 5) {
                unsigned delay = rand() % 100; // NOLINT
                defaultThreadPool.execute(
                    new TestTask("Running ...", result, delay));

#ifdef USE_BUSY_TEST
                // FIXME! CK BOOST_TEST(defaultThreadPool.is_busy());
#endif
            }
            BOOST_TEST_MESSAGE("defaultThreadPool.queue_length: "
                << defaultThreadPool.queue_length());
        } while (--i > 0);

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(100); // ms
        } while (!defaultThreadPool.is_idle());

#ifdef USE_BUSY_TEST
        BOOST_TEST(!defaultThreadPool.is_busy());
#endif

        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST_MESSAGE("executed tasks: " << TestTask::run_count());

        // NOTE: implicit called: defaultThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 16UL, "All task has to be executed!");
    TestTask::reset_counter();
}

#ifndef USE_AGENTPP
BOOST_AUTO_TEST_CASE(QueuedThreadPoolInterface_test)
{
    result_queue_t result;
    {
        QueuedThreadPool emptyThreadPool(
            0UL, 0x20000); // NOTE: without any worker thread! CK
        BOOST_TEST(emptyThreadPool.size() == 0UL);

#    if defined(USE_AGENTPP)
        BOOST_TEST(emptyThreadPool.is_idle());
        // NOTE: NO! XXX CK emptyThreadPool.terminate();
        // NOTE: NO! XXX CK BOOST_TEST(!emptyThreadPool.is_idle());

        // TODO: not clear! CK
        emptyThreadPool.set_stack_size(
            0x20000); // NOTE: this change the queue thread only! CK
        BOOST_TEST(emptyThreadPool.stack_size() == 0x20000);
#    endif

        BOOST_TEST_MESSAGE("emptyThreadPool.size: " << emptyThreadPool.size());
        emptyThreadPool.execute(new TestTask("I want to run!", result));

#    ifdef USE_BUSY_TEST
        BOOST_TEST(emptyThreadPool.is_busy());
#    endif

#    if !defined(AGENTPP_USE_IMPLIZIT_START)
        emptyThreadPool.start();
        emptyThreadPool.execute(new TestTask("after Starting ...", result));
#    endif

        // NOTE: NO! XXX CK BOOST_TEST(!emptyThreadPool.is_idle());

        size_t i = 10;
        do {
            if (i > 5) {
                emptyThreadPool.execute(new TestTask("Running ...", result));
            }
            BOOST_TEST_MESSAGE("emptyThreadPool.queue_length: "
                << emptyThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        // NOTE: NO! XXX CK BOOST_TEST(!emptyThreadPool.is_idle());
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        // NOTE: NO! XXX CK BOOST_TEST(TestTask::task_count() == 6UL);

        // NOTE: implicit called: emptyThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 0UL, "NO task has to be executed!");
    TestTask::reset_counter();
}
#endif

BOOST_AUTO_TEST_CASE(QueuedThreadPoolIndependency_test)
{

    result_queue_t result;
    QueuedThreadPool firstThreadPool(1);
    BOOST_TEST(firstThreadPool.size() == 1UL);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
    firstThreadPool.start();
#endif

    BOOST_TEST_MESSAGE("firstThreadPool.size: " << firstThreadPool.size());
    firstThreadPool.execute(new TestTask("Starting ...", result));

#ifdef USE_BUSY_TEST
    // FIXME! CK BOOST_TEST(firstThreadPool.is_busy());
#endif

    size_t n = 1;
    {
        QueuedThreadPool secondThreadPool(4);
        BOOST_TEST_MESSAGE(
            "secondThreadPool.size: " << secondThreadPool.size());

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        secondThreadPool.start();
#endif

        BOOST_TEST(secondThreadPool.is_idle());
        secondThreadPool.execute(new TestTask("Starting ...", result));
        n++;

#ifdef USE_BUSY_TEST
        while (secondThreadPool.is_busy()) {
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        }
        BOOST_TEST(!secondThreadPool.is_busy());
#else
        do {
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!secondThreadPool.is_idle());
        BOOST_TEST(secondThreadPool.is_idle());
#endif

        secondThreadPool.terminate();
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms

#ifdef TEST_USAGE_AFTER_TERMINATE
        secondThreadPool.execute(new TestTask("After terminate ...", result));
        // NOTE: NO! XXX CK n++;

        size_t i = 10;
        do {
            if (i > 5) {
                secondThreadPool.execute(new TestTask("Queuing ...", result));
                // NOTE: NO! XXX CK n++;
            }
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (--i > 0);
#endif

        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST(TestTask::run_count() == n);
    }

    firstThreadPool.execute(new TestTask("Stopping ...", result));
    n++;
    BOOST_TEST_MESSAGE(
        "firstThreadPool.queue_length: " << firstThreadPool.queue_length());
    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms

#ifdef USE_BUSY_TEST
    while (firstThreadPool.is_busy()) {
        BOOST_TEST(!firstThreadPool.is_idle());
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    }
#else
    do {
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    } while (!firstThreadPool.is_idle());
#endif

    BOOST_TEST(firstThreadPool.is_idle());
    firstThreadPool.terminate();

    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());

    //############################
    // FIXME! outstanding tasks: 2? CK
    // BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be
    // deleted!");
    BOOST_TEST(TestTask::run_count() == n, "All task has to be executed!");
    //############################

    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(Synchronized_test)
{
    Synchronized sync;
    {
        BOOST_TEST(sync.lock());
        BOOST_TEST(sync.unlock());

#ifndef __linux__ // ThreadSanitizer: unlock of an unlocked mutex (or by a
                  // wrong thread)
        BOOST_TEST(!sync.unlock(), "second unlock() returns OK");
#endif
    }

#ifndef __linux__ // ThreadSanitizer: unlock of an unlocked mutex (or by a
                  // wrong thread)
    BOOST_TEST(!sync.unlock(), "unlock() without previous lock() returns OK");
#endif
}

BOOST_AUTO_TEST_CASE(SyncTrylock_test)
{
    Synchronized sync;
    BOOST_TEST(sync.trylock() == Synchronized::LOCKED);
    sync.unlock();
    {
        Lock l(sync);

#if defined(USE_AGENTPP_CK) || defined(USE_AGENTPP)
        BOOST_TEST(sync.trylock() == Synchronized::BUSY);
#else
        BOOST_TEST(sync.trylock() == Synchronized::OWNED);
#endif
    }

#ifndef __linux__ // ThreadSanitizer: unlock of an unlocked mutex (or by a
                  // wrong thread)
    BOOST_TEST(!sync.unlock(), "second unlock() returns OK");
#endif
}

BOOST_AUTO_TEST_CASE(SyncDeadlock_test)
{
    Synchronized sync;
    try {
        Lock l(sync);
        BOOST_TEST(!sync.lock());
    } catch (std::exception& e) {
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);
        BOOST_TEST_MESSAGE(e.what());
    }
    BOOST_TEST(sync.lock());
    BOOST_TEST(sync.unlock());
}

void handler(int signum)
{
    switch (signum) {
    case SIGALRM:
        signal(signum, SIG_DFL);
        break;
    default: // ignored
        break;
    }
}

BOOST_AUTO_TEST_CASE(SyncDeleteLocked_test)
{
#ifdef __APPLE__
    signal(SIGALRM, &handler);
    ualarm(1000, 0); // us
#endif

    Stopwatch sw;
    try {

        auto sync = boost::make_shared<Synchronized>();
        BOOST_TEST(sync->lock());

#ifdef __APPLE__
        sync->wait(123); // for signal with timout
#endif

        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
    } catch (std::exception& e) {
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);
        BOOST_TEST_MESSAGE(e.what());
    }
}

#ifndef _WIN32
BOOST_AUTO_TEST_CASE(SyncWait_test)
{
    Synchronized sync;
    {
        Lock l(sync);
        Stopwatch sw;

#    ifdef USE_WAIT_FOR
        BOOST_TEST(!sync.wait_for(BOOST_THREAD_TEST_TIME_MS),
            "no timeout occurred on wait!");
#    else
        BOOST_TEST(sync.wait(BOOST_THREAD_TEST_TIME_MS),
            "no timeout occurred on wait!");
#    endif // !defined(USE_WAIT_FOR)

        ns d = sw.elapsed();
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d >= ms(BOOST_THREAD_TEST_TIME_MS - 1));
        // TODO: error: in "SyncWait_test": check d >= ms(75) has failed
        // [74892559 nanoseconds < 75 milliseconds]
    }
}
#endif

class BadTask : public Runnable {
private:
    Synchronized sync;
    boost::atomic<bool> stopped { false };
    const bool doit;

public:
    explicit BadTask(bool do_throw)
        : sync()
        , doit(do_throw) {};

    ~BadTask()
    {
        stopped = true;
        sync.notify_all();
    }

    void stop()
    {
        Lock l(sync); // wait for the scoped lock
        stopped = true;
        sync.notify();
    }

    void run() BOOST_OVERRIDE
    {
        Lock l(sync); // wait for the scoped lock

        start_latch.wait();

        // WARNING: ThreadSanitizer: data race:
        // BOOST_TEST_MESSAGE(
        //     BOOST_CURRENT_FUNCTION << ": Hello world! I'am waiting ...");

        if (doit) {
            throw std::runtime_error("Fatal Error, can't continue!");
        }

        while (!stopped) {
            sync.wait();                 // wait for the stop signal ..
            Thread::sleep(rand() % 113); // NOLINT
        }
    };

#ifdef USE_UNIQUE_PTR
    boost::unique_ptr<Runnable> clone() const BOOST_OVERRIDE
    {
        return boost::make_unique<BadTask>();
    }
#endif
};

//==================================================

#ifndef USE_AGENTPP
BOOST_AUTO_TEST_CASE(ThreadTaskThrow_test)
{
    Stopwatch sw;
    {
        BadTask task(true);
        Thread thread(task);

        start_latch.reset(1);
        thread.start(); // first the task will wait for the lock
        start_latch.count_down();
        boost::this_thread::yield();

        BOOST_TEST(thread.is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}
#endif

BOOST_AUTO_TEST_CASE(ThreadTaskJoin_test)
{
    Stopwatch sw;
    {
        BadTask task(false);
        Thread thread(task);

        start_latch.reset(1);
        thread.start();
        start_latch.count_down();
        boost::this_thread::yield();

        task.stop();

        BOOST_TEST(thread.is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}

BOOST_AUTO_TEST_CASE(ThreadLivetime_test)
{
    Stopwatch sw;
    {
        Thread thread;
        boost::shared_ptr<Thread> ptrThread(thread.clone());
        thread.start();
        BOOST_TEST(thread.is_alive());
        thread.join();
        BOOST_TEST(!ptrThread->is_alive());
        ptrThread->join();
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}

BOOST_AUTO_TEST_CASE(ThreadNanoSleep_test)
{
#ifndef _WIN32
    signal(SIGALRM, &handler);
    alarm(1); // s
#endif

    {
        Stopwatch sw;
        Thread::sleep(1234, 999); // ms + ns
        ns d = sw.elapsed();
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
#ifndef _WIN32
        BOOST_TEST(d >= (ms(1234) + ns(999)));
#endif
    }

    {
        Stopwatch sw;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS, 999999); // ms + ns
        ns d = sw.elapsed();
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
#ifndef _WIN32
        BOOST_TEST(d >= (ms(BOOST_THREAD_TEST_TIME_MS) + ns(999999)));
#endif
    }
}

BOOST_AUTO_TEST_CASE(ThreadSleep_test)
{
    Stopwatch sw;
    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // 75 ms -> 75000000 nanoseconds
    ns d = sw.elapsed();
    BOOST_TEST_MESSAGE(
        BOOST_CURRENT_FUNCTION << sw.elapsed()); // i.e.: 168410479 nanoseconds
#ifndef _WIN32
    BOOST_TEST(d >= ns(BOOST_THREAD_TEST_TIME_MS));
#endif
}

#ifdef USE_WAIT_FOR
struct wait_data {

#    ifndef TEST_INDEPENDENTLY
    typedef Synchronized lockable_type;
#    else
    typedef boost::mutex lockable_type;
#    endif

    typedef boost::unique_lock<lockable_type> scoped_lock;

    bool flag;
    lockable_type mtx;
    boost::condition_variable cond;

    wait_data()
        : flag(false)
    { }

    // NOTE: return false if condition waiting for is not true! CK
    bool predicate() const { return flag; }

    void wait()
    {
        scoped_lock l(mtx);
#    ifndef TEST_INDEPENDENTLY
        mtx.wait();
#    else
        while (!predicate()) {
            cond.wait(l);
        }
#    endif
    }

    // Returns: false if the call is returning because the time specified by
    // abs_time was reached, true otherwise.
    template <typename Duration> bool timed_wait(Duration d)
    {
        scoped_lock l(mtx);
#    ifndef TEST_INDEPENDENTLY
        return mtx.wait_for(ms(d).count());
#    else
        while (!predicate()) {
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                return false;
            }
        }
        return true; // OK
#    endif
    }

    void signal()
    {
        scoped_lock l(mtx);
#    ifndef TEST_INDEPENDENTLY
        mtx.notify_all();
#    else
        flag = true;
        cond.notify_all();
#    endif
    }
};

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);
typedef Synchronized mutex_type;

void lock_mutexes_slowly(
    mutex_type* m1, mutex_type* m2, wait_data* locked, wait_data* quit)
{
    using namespace boost;

    lock_guard<mutex_type> l1(*m1);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l2(*m2);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    locked->signal();
    quit->wait();
}

void lock_pair(mutex_type* m1, mutex_type* m2)
{
    using namespace boost;

    lock(*m1, *m2);
    unique_lock<mutex_type> l1(*m1, adopt_lock), l2(*m2, adopt_lock);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    BOOST_TEST(l1.owns_lock());
    BOOST_TEST(l2.owns_lock());
}

BOOST_AUTO_TEST_CASE(test_lock_two_other_thread_locks_in_order)
{
    using namespace boost;

    mutex_type m1, m2;
    wait_data locked;
    wait_data release;

    thread t(lock_mutexes_slowly, &m1, &m2, &locked, &release);

    thread t2(lock_pair, &m1, &m2);
    BOOST_TEST(locked.timed_wait(ms(2 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(t2.try_join_for(ms(4 * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

BOOST_AUTO_TEST_CASE(test_lock_two_other_thread_locks_in_opposite_order)
{
    using namespace boost;

    mutex_type m1, m2;
    wait_data locked;
    wait_data release;

    thread t(lock_mutexes_slowly, &m1, &m2, &locked, &release);

    thread t2(lock_pair, &m2, &m1); // NOTE: m2 first!
    BOOST_TEST(locked.timed_wait(ms(2 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(t2.try_join_for(ms(4 * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

void lock_five_mutexes_slowly(mutex_type* m1, mutex_type* m2, mutex_type* m3,
    mutex_type* m4, mutex_type* m5, wait_data* locked, wait_data* quit)
{
    using namespace boost;

    lock_guard<mutex_type> l1(*m1);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l2(*m2);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l3(*m3);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l4(*m4);
    this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS));
    lock_guard<mutex_type> l5(*m5);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    locked->signal();
    quit->wait();
}

void lock_n(mutex_type* mutexes, unsigned count)
{
    using namespace boost;

    lock(mutexes, mutexes + count);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    if (count == 1) {
        Stopwatch sw;
        BOOST_TEST(mutexes[0].wait_for(BOOST_THREAD_TEST_TIME_MS));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(mutexes[0].unlock());
        return;
    }

    for (unsigned i = 0; i < count; ++i) {
        ms d(BOOST_THREAD_TEST_TIME_MS);
        this_thread::sleep_for(d);
        BOOST_TEST(mutexes[i].unlock());
    }
}

BOOST_AUTO_TEST_CASE(test_lock_ten_other_thread_locks_in_different_order)
{
    using namespace boost;
    unsigned const num_mutexes = 10;

    mutex_type mutexes[num_mutexes];
    wait_data locked;
    wait_data release;

    thread t(lock_five_mutexes_slowly, &mutexes[6], &mutexes[3], &mutexes[8],
        &mutexes[0], &mutexes[2], &locked, &release);

    thread t2(lock_n, mutexes, num_mutexes);
    BOOST_TEST(locked.timed_wait(ms(2 * 5 * BOOST_THREAD_TEST_TIME_MS)));

    release.signal();

    BOOST_TEST(
        t2.try_join_for(ms(2 * num_mutexes * BOOST_THREAD_TEST_TIME_MS)));
    t2.join(); // just in case of timeout! CK

    t.join();
}

BOOST_AUTO_TEST_CASE(SyncTry_lock_for_test)
{
    unsigned const num_mutexes = 2;

    Synchronized timed_locks[num_mutexes];
    {
        const unsigned timeout = BOOST_THREAD_TEST_TIME_MS / num_mutexes;
        boost::thread t1(lock_n, timed_locks, num_mutexes);
        boost::this_thread::sleep_for(ms(timeout));

        Stopwatch sw;
        BOOST_TEST(
            !timed_locks[1].lock(timeout), "no timeout occurred on lock!");
        ns d = sw.elapsed() - ms(timeout);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
        t1.join();
    }
    {
        Stopwatch sw;
        BOOST_TEST(timed_locks[0].lock(BOOST_THREAD_TEST_TIME_MS),
            "timeout occurred on lock!");
        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST(d < ns(max_diff));
        BOOST_TEST(timed_locks[0].unlock());
    }
}

BOOST_AUTO_TEST_CASE(SyncDelete_while_used_test)
{
    unsigned const num_mutexes = 1;

    Synchronized* lockable = new Synchronized;
    boost::thread t1(lock_n, lockable, num_mutexes);
    {
        Stopwatch sw;
        boost::this_thread::sleep_for(ms(BOOST_THREAD_TEST_TIME_MS / 2));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        delete lockable;
    }
    t1.join();
    {
        Stopwatch sw;
        Synchronized* lockable = new Synchronized;
        lockable->lock();
        delete lockable;
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
    }
}
#endif // !defined(USE_AGENTPP_CK)

#ifndef _WIN32
int main(int argc, char* argv[])
{
    // prototype for user's unit test init function
    extern ::boost::unit_test::test_suite* init_unit_test_suite(
        int argc, char* argv[]);
    int loops                                       = 1;
    int error                                       = 0;
    boost::unit_test::init_unit_test_func init_func = &init_unit_test_suite;
    std::vector<std::string> args;

    if (argc > 1 && argv[argc - 1][0] == '-') {
        std::stringstream ss(argv[argc - 1] + 1);
        ss >> loops;
        if (!ss.fail()) {
            std::cout << "loops requested: " << loops << std::endl;
            --argc; // private args not for boost::unit_test! CK
        }
        for (int i = 0; i < argc; ++i) {
            args.push_back(std::string(argv[i]));
            std::cout << i << ": " << argv[i] << std::endl;
        }
    }

    do {
        StopwatchReporter sw;
        srand(time(NULL));

        error = ::boost::unit_test::unit_test_main(init_func, argc, argv);

        if (--loops <= 0) {
            break;
        }
        for (int i = 0; i < argc; ++i) {
            strncpy(argv[i], args[i].c_str(), args[i].length());
            std::cout << i << ": " << argv[i] << std::endl;
        }
        std::cout << "loops left: " << loops << std::endl;
    } while (!error);

    return error;
}
#endif
