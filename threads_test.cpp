//
// test program for agentpp threadpool
//
// astyle --style=kr thread*.{cpp,hpp}
// clang-format -style=file -i thread*.{cpp,hpp}
//
#define TEST_USAGE_AFTER_TERMINATE

#ifdef USE_AGENTPP
#    include "agent_pp/threads.h" // ThreadPool, QueuedThreadPool
using namespace Agentpp;
#elif USE_AGENTPP_CK
#    include "possix/threadpool.hpp" // ThreadPool, QueuedThreadPool
#    define TEST_INDEPENDENTLY
using namespace AgentppCK;
#else
#    include "threadpool.hpp" // ThreadPool, QueuedThreadPool
using namespace Agentpp;
#endif

#include "simple_stopwatch.hpp"

// -----------------------------------------
#define USE_BUSY_TEST
#define BOOST_TEST_MODULE Threads
#define BOOST_TEST_NO_MAIN
#include <boost/test/included/unit_test.hpp>
// -----------------------------------------

#include <boost/atomic.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_only.hpp>

#include <iostream>
#include <string>
#include <vector>

#if !defined BOOST_THREAD_TEST_TIME_MS
#    ifdef BOOST_THREAD_PLATFORM_WIN32
#        define BOOST_THREAD_TEST_TIME_MS 100
#    else
#        define BOOST_THREAD_TEST_TIME_MS 75
#    endif
#endif

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

typedef size_t test_counter_t;
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

#ifndef USE_AGENTPP_CK
    virtual std::unique_ptr<Runnable> clone() const BOOST_OVERRIDE
    {
        return std::make_unique<TestTask>(text, result, delay);
    }
#endif

    virtual void run() BOOST_OVERRIDE
    {
        Thread::sleep((rand() % 3) * delay); // ms

        scoped_lock l(lock);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << " called with: " << text);
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
    unsigned delay;
};

TestTask::lockable_type
    TestTask::lock; //  warning: initialization of 'lock' with static storage
                    //  duration may throw an exception that cannot be caught
                    //  [cert-err58-cpp]

test_counter_t TestTask::run_cnt(0);
test_counter_t TestTask::counter(0);

void push_task(ThreadPool* tp)
{
    static result_queue_t result;
    tp->execute(new TestTask(
        "Generate to mutch load.", result, BOOST_THREAD_TEST_TIME_MS));
}

BOOST_AUTO_TEST_CASE(ThreadPool_busy_test)
{
    {
        const size_t stacksize = AGENTPP_DEFAULT_STACKSIZE * 2;
        const size_t threadCount(2UL);
        ThreadPool threadPool(threadCount, stacksize);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == threadCount);
        BOOST_TEST(threadPool.get_stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());
        BOOST_TEST(!threadPool.is_busy());

        // call execute parallel from different task!
        std::array<boost::thread, threadCount> threads;
        for (size_t i = 0; i < threadCount; ++i) {
            threads.at(i) = boost::thread(push_task, &threadPool);
            threads.at(i).detach();
            boost::this_thread::yield();
        }
        boost::this_thread::yield();

#ifdef USE_BUSY_TEST
        // XXX BOOST_TEST(threadPool.is_busy());

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
#endif
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(ThreadPool_test)
{
    result_queue_t result;
    {
        ThreadPool threadPool(4UL);

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 4UL);
        BOOST_TEST(threadPool.get_stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(threadPool.is_idle());

        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("Hallo world!", result));
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("ThreadPool is running!", result));
        BOOST_TEST(!threadPool.is_busy());
        threadPool.execute(new TestTask("Generate some load.", result));
        BOOST_TEST(!threadPool.is_busy());

        threadPool.execute(new TestTask("Under full load now!", result));
        threadPool.execute(new TestTask("Good by!", result));

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        //############################
        BOOST_TEST(TestTask::task_count() == 0UL);
        //############################
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 5UL, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_busy_test)
{
    result_queue_t result;
    {
        const size_t stacksize = AGENTPP_DEFAULT_STACKSIZE * 2;
        QueuedThreadPool threadPool(2UL, stacksize);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        threadPool.start(); // NOTE: different to ThreadPool, but this
                            // should not really needed!
#endif

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 2UL);
        BOOST_TEST(threadPool.get_stack_size() == stacksize);
        BOOST_TEST(threadPool.is_idle());
        BOOST_TEST(!threadPool.is_busy());

        // call execute parallel from different task!
        std::array<boost::thread, 4> threads;
        for (int i = 0; i < 4; ++i) {
            threads.at(i) = boost::thread(push_task, &threadPool);
            threads.at(i).detach();
        }

#ifdef USE_BUSY_TEST
        // XXX BOOST_TEST(threadPool.is_busy());

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (!threadPool.is_idle());
        BOOST_TEST(threadPool.is_idle());

        threadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
#endif
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPool_test)
{
    result_queue_t result;
    {
        QueuedThreadPool queuedThreadPool;

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        queuedThreadPool.start(); // NOTE: different to ThreadPool, but this
                                  // should not really needed!
#endif

        BOOST_TEST_MESSAGE(
            "queuedThreadPool.size: " << queuedThreadPool.size());
        BOOST_TEST(queuedThreadPool.size() == 1UL);

        BOOST_TEST(
            queuedThreadPool.get_stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_TEST(queuedThreadPool.is_idle());
        BOOST_TEST(!queuedThreadPool.is_busy());

        queuedThreadPool.execute(new TestTask("1 Hi again.", result, 10));
        queuedThreadPool.execute(
            new TestTask("2 Queuing starts.", result, 20));
        queuedThreadPool.execute(
            new TestTask("3 Under full load!", result, 30));

#ifdef USE_BUSY_TEST
        BOOST_TEST(!queuedThreadPool.is_idle());
        // XXX BOOST_TEST(queuedThreadPool.is_busy());
#endif

        std::srand(static_cast<unsigned>(std::time(0)));
        unsigned i = 4;
        do {
            unsigned delay = rand() % 100;
            std::string msg(std::to_string(i));
            queuedThreadPool.execute(
                new TestTask(msg + " Queuing ...", result, delay));
        } while (++i < (4 + 6));

        do {
            BOOST_TEST_MESSAGE("queuedThreadPool.queue_length: "
                << queuedThreadPool.queue_length());
            Thread::sleep(500); // NOTE: after more than 1/2 sec! CK
        } while (!queuedThreadPool.is_idle());

        BOOST_TEST(queuedThreadPool.is_idle());
        BOOST_TEST(!queuedThreadPool.is_busy());

        queuedThreadPool.terminate();
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        //############################
        BOOST_TEST(TestTask::task_count() == 0UL);
        //############################
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
                //###XXX### BOOST_TEST_MESSAGE("expected msg: " << msg);
                BOOST_TEST(boost::hash_value(value) == boost::hash_value(msg),
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
        BOOST_TEST(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE(
            "defaultThreadPool.size: " << defaultThreadPool.size());
        defaultThreadPool.execute(new TestTask("Started ...", result));

        unsigned i = 20;
        do {
            if (i > 5) {
                unsigned delay = rand() % 100;
                defaultThreadPool.execute(
                    new TestTask("Running ...", result, delay));
                // TODO BOOST_TEST(!defaultThreadPool.is_idle());
                Thread::sleep(2 * delay); // ms
            }
            BOOST_TEST_MESSAGE("defaultThreadPool.queue_length: "
                << defaultThreadPool.queue_length());
        } while (--i > 0);

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(100); // ms
        } while (defaultThreadPool.is_busy());

        BOOST_TEST(defaultThreadPool.is_idle());
        BOOST_TEST(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST_MESSAGE("executed tasks: " << TestTask::run_count());

        // NOTE: implicit done: defaultThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 16UL, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolInterface_test)
{
    result_queue_t result;
    {
        QueuedThreadPool emptyThreadPool(
            0UL, 0x20000); // NOTE: without any worker thread! CK
        BOOST_TEST(emptyThreadPool.size() == 0UL);

#if !defined(USE_AGENTPP) && defined(AGENTPP_USE_IMPLIZIT_START)
        BOOST_TEST(emptyThreadPool.is_idle());
        // XXX NO! emptyThreadPool.stop();
        // XXX NO! BOOST_TEST(!emptyThreadPool.is_idle());
#endif

        emptyThreadPool.set_stack_size(
            0x20000); // NOTE: this change the queue thread only! CK
        BOOST_TEST(emptyThreadPool.get_stack_size() == 0x20000);

        BOOST_TEST_MESSAGE("emptyThreadPool.size: " << emptyThreadPool.size());
        emptyThreadPool.execute(new TestTask("Starting ...", result));
        BOOST_TEST(emptyThreadPool.is_busy());

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        emptyThreadPool.start();
#endif

        // XXX NO! BOOST_TEST(!emptyThreadPool.is_idle());

        size_t i = 10;
        do {
            if (i > 5) {
                emptyThreadPool.execute(new TestTask("Running ...", result));
            }
            BOOST_TEST_MESSAGE("emptyThreadPool.queue_length: "
                << emptyThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        // XXX NO! BOOST_TEST(!emptyThreadPool.is_idle());
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        // XXX NO! BOOST_TEST(TestTask::task_count() == 6UL);

        // NOTE: implicit done: emptyThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 0UL, "NO task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolIndependency_test)
{

    result_queue_t result;
    QueuedThreadPool firstThreadPool(1);
    BOOST_TEST(firstThreadPool.size() == 1UL);

#if !defined(AGENTPP_USE_IMPLIZIT_START)
    firstThreadPool.start();
#endif

    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    BOOST_TEST_MESSAGE("firstThreadPool.size: " << firstThreadPool.size());
    firstThreadPool.execute(new TestTask("Starting ...", result));

    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    BOOST_TEST(firstThreadPool.is_idle());

    size_t n = 1;
    {
        QueuedThreadPool secondThreadPool(4);
        BOOST_TEST_MESSAGE(
            "secondThreadPool.size: " << secondThreadPool.size());

#if !defined(AGENTPP_USE_IMPLIZIT_START)
        secondThreadPool.start();
#endif

        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        BOOST_TEST(secondThreadPool.is_idle());

        secondThreadPool.execute(new TestTask("Starting ...", result));
        n++;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        if (secondThreadPool.is_busy()) {
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        }
        BOOST_TEST(secondThreadPool.is_idle());

        secondThreadPool.terminate();
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms

#ifdef TEST_USAGE_AFTER_TERMINATE
        secondThreadPool.execute(new TestTask("After terminate ...", result));
        // NO! n++; // XXX

        size_t i = 10;
        do {
            if (i > 5) {
                secondThreadPool.execute(new TestTask("Queuing ...", result));
                // NO! n++; // XXX
            }
            Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        } while (--i > 0);
#endif

        if (secondThreadPool.is_busy()) {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
        }

        BOOST_TEST(TestTask::run_count() == n);
    }

    firstThreadPool.execute(new TestTask("Stopping ...", result));
    n++;
    BOOST_TEST_MESSAGE(
        "firstThreadPool.queue_length: " << firstThreadPool.queue_length());
    Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    if (firstThreadPool.is_busy()) {
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
    }
    BOOST_TEST(firstThreadPool.is_idle());
    firstThreadPool.terminate();

    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == n, "All task has to be executed!");

    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(Synchronized_test)
{
    Synchronized sync;
    {
        BOOST_TEST(sync.lock());
        BOOST_TEST(sync.unlock());
        BOOST_TEST(!sync.unlock(), "second unlock() returns no error");
    }
    BOOST_TEST(
        !sync.unlock(), "unlock() without previous lock() returns no error");
}

BOOST_AUTO_TEST_CASE(SyncTrylock_test)
{
    Synchronized sync;
    {
        Lock l(sync);

#if !defined(USE_AGENTPP_CK)
        BOOST_TEST(sync.trylock() == Synchronized::OWNED);
#else
        BOOST_TEST(sync.trylock() == Synchronized::BUSY);
#endif // !defined(USE_AGENTPP_CK)
    }
    BOOST_TEST(!sync.unlock(), "second unlock() returns no error");
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
        // XXX BOOST_TEST(false);
    }
    BOOST_TEST(sync.lock());
    BOOST_TEST(sync.unlock());
}

BOOST_AUTO_TEST_CASE(SyncWait_test)
{
    Synchronized sync;
    {
        Lock l(sync);
        Stopwatch sw;
#if !defined(USE_AGENTPP_CK)
        BOOST_TEST(!sync.wait_for(BOOST_THREAD_TEST_TIME_MS),
            "no timeout occurred on wait!");
#else
        BOOST_TEST(sync.wait(BOOST_THREAD_TEST_TIME_MS),
            "no timeout occurred on wait!");
#endif // !defined(USE_AGENTPP_CK)

        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
}

class BadTask : public Runnable {
public:
    BadTask() {};
    virtual void run() BOOST_OVERRIDE
    {
        std::cout << "Hello world!" << std::endl;
        throw std::runtime_error("Fatal Error, can't continue!");
    };

#ifndef USE_AGENTPP_CK
    virtual std::unique_ptr<Runnable> clone() const BOOST_OVERRIDE
    {
        return std::make_unique<BadTask>();
    }
#endif
};

#ifndef USE_AGENTPP_CK
BOOST_AUTO_TEST_CASE(ThreadTaskThrow_test)
{
    Stopwatch sw;
    {
        Thread thread(new BadTask());
        thread.start();
        BOOST_TEST(thread.is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}
#endif

#if 0
BOOST_AUTO_TEST_CASE(ThreadLivetime_test)
{
    Stopwatch sw;
    {
        Thread thread;
        boost::shared_ptr<Thread> ptrThread(thread.clone());
        thread.start();
        BOOST_TEST(thread.is_alive());
        BOOST_TEST(!ptrThread->is_alive());
    }
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}
#endif

BOOST_AUTO_TEST_CASE(ThreadNanoSleep_test)
{
    Stopwatch sw;
    Thread::sleep(1, 999);
    ns d = sw.elapsed() - ms(2);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
    BOOST_TEST(d < ns(max_diff));
}

BOOST_AUTO_TEST_CASE(ThreadSleep_test)
{
    {
        Stopwatch sw;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS); // ms
        ns d = sw.elapsed() - ms(BOOST_THREAD_TEST_TIME_MS);
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
    {
        Stopwatch sw;
        Thread::sleep(BOOST_THREAD_TEST_TIME_MS, 999999); // ms + ns
        ns d = sw.elapsed() - (ms(BOOST_THREAD_TEST_TIME_MS) + ns(999999));
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
        BOOST_TEST(d < ns(max_diff));
    }
}

struct wait_data {

#ifndef TEST_INDEPENDENTLY
    typedef Synchronized lockable_type;
#else
    typedef boost::mutex lockable_type;
#endif

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
#ifndef TEST_INDEPENDENTLY
        mtx.wait();
#else
        while (!predicate()) {
            cond.wait(l);
        }
#endif
    }

    // Returns: false if the call is returning because the time specified by
    // abs_time was reached, true otherwise.
    template <typename Duration> bool timed_wait(Duration d)
    {
        scoped_lock l(mtx);
#ifndef TEST_INDEPENDENTLY
        return mtx.wait_for(ms(d).count());
#else
        while (!predicate()) {
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                return false;
            }
        }
        return true; // OK
#endif
    }

    void signal()
    {
        scoped_lock l(mtx);
#ifndef TEST_INDEPENDENTLY
        mtx.notify_all();
#else
        flag = true;
        cond.notify_all();
#endif
    }
};


#ifndef USE_AGENTPP_CK
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
