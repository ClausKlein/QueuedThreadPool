//
// test program for agentpp threadpool
//
// astyle --style=kr thread*.{cpp,hpp}
// clang-format -style=file -i thread*.{cpp,hpp}
//


#ifdef USE_AGENTPP
#include "agent_pp/threads.h" // ThreadPool, QueuedThreadPool
#else
#include "threadpool.hpp" // ThreadPool, QueuedThreadPool
#endif


#define BOOST_TEST_MODULE Threads
#define BOOST_TEST_NO_MAIN
#include <boost/test/included/unit_test.hpp>

#include <boost/atomic.hpp>
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/queue.hpp>

#include "boost/testable_mutex.hpp"
#include "simple_stopwatch.hpp"

#include <iostream>
#include <string>
#include <vector>


typedef boost::atomic_size_t test_counter_t;
typedef boost::lockfree::queue<size_t, boost::lockfree::capacity<20> >
    result_queue_t;

class TestTask : public Agentpp::Runnable {
public:
    explicit TestTask(
        const std::string& msg, result_queue_t& rslt, unsigned ms_delay = 11)
        : text(msg)
        , result(rslt)
        , delay(ms_delay)
    {
        // TODO use boost::mutex! Agentpp::Lock l(lock);
        ++counter;
    }

    virtual ~TestTask()
    {
        // TODO use boost::mutex! Agentpp::Lock l(lock);
        --counter;
    }

    virtual void run()
    {
        Agentpp::Thread::sleep(delay); // ms

        // TODO use boost::mutex! Agentpp::Lock l(lock);
        // TODO BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << " called with: "
        // << text);
        size_t hash = boost::hash_value(text);
        result.push(hash);
        ++run_cnt;
    }

    static size_t run_count()
    {
        // TODO use boost::mutex! Agentpp::Lock l(lock);
        return run_cnt;
    }
    static size_t task_count()
    {
        // TODO use boost::mutex! Agentpp::Lock l(lock);
        return counter;
    }
    static void reset_counter()
    {
        // TODO use boost::mutex! Agentpp::Lock l(lock);
        counter = 0;
        run_cnt = 0;
    }

protected:
    static test_counter_t run_cnt;
    static test_counter_t counter;
    // NOTE: not longer used! static Agentpp::Synchronized lock;

private:
    const std::string text;
    result_queue_t& result;
    unsigned delay;
};

// TODO use boost::mutex! Agentpp::Synchronized TestTask::lock;
test_counter_t TestTask::run_cnt(0);
test_counter_t TestTask::counter(0);


BOOST_AUTO_TEST_CASE(ThreadPool_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        Agentpp::ThreadPool threadPool(4UL);
        //###XXX###: missing! threadPool.start();   //NOTE: implicit done! CK

        BOOST_TEST_MESSAGE("threadPool.size: " << threadPool.size());
        BOOST_TEST(threadPool.size() == 4UL);
        BOOST_CHECK(threadPool.stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_CHECK(threadPool.is_idle());

        BOOST_CHECK(!threadPool.is_busy());
        threadPool.execute(new TestTask("Hallo world!", result));
        BOOST_CHECK(!threadPool.is_busy());
        threadPool.execute(new TestTask("ThreadPool is running!", result));
        BOOST_CHECK(!threadPool.is_busy());
        threadPool.execute(new TestTask("Generate some load.", result));
        BOOST_CHECK(!threadPool.is_busy());
        threadPool.execute(new TestTask("Under full load now!", result));
        BOOST_CHECK(threadPool.is_busy());
        threadPool.execute(new TestTask("Good by!", result));

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(123); // ms
        } while (!threadPool.is_idle());
        BOOST_CHECK(threadPool.is_idle());

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

BOOST_AUTO_TEST_CASE(QueuedThreadPool_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool queuedThreadPool(1UL);

#if !defined(USE_IMPLIZIT_START)
        queuedThreadPool.start(); // NOTE: different to ThreadPool, but this
                                  // should not really needed!
#endif

        BOOST_TEST_MESSAGE(
            "queuedThreadPool.size: " << queuedThreadPool.size());
        BOOST_TEST(queuedThreadPool.size() == 1UL);
        BOOST_TEST(queuedThreadPool.stack_size() == AGENTPP_DEFAULT_STACKSIZE);
        BOOST_CHECK(queuedThreadPool.is_idle());
        BOOST_CHECK(!queuedThreadPool.is_busy());

        queuedThreadPool.execute(new TestTask("1 Hi again.", result, 10));
        queuedThreadPool.execute(
            new TestTask("2 Queuing starts.", result, 20));
        queuedThreadPool.execute(
            new TestTask("3 Under full load!", result, 30));
        BOOST_CHECK(!queuedThreadPool.is_idle());
        BOOST_CHECK(queuedThreadPool.is_busy());

        std::srand(static_cast<unsigned>(
            std::time(0))); // use current time as seed for random generator
        unsigned i = 4;
        do {
            unsigned delay = std::rand() % 100;
            std::string msg(boost::lexical_cast<std::string>(i));
            queuedThreadPool.execute(
                new TestTask(msg + " Queuing ...", result, delay));
        } while (++i < (4 + 6));

        do {
            BOOST_TEST_MESSAGE("queuedThreadPool.queue_length: "
                << queuedThreadPool.queue_length());
            Thread::sleep(500); // NOTE: after more than 1/2 sec! CK
        } while (!queuedThreadPool.is_idle());
        BOOST_CHECK(queuedThreadPool.is_idle());
        BOOST_CHECK(!queuedThreadPool.is_busy());

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
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool defaultThreadPool;

#if !defined(USE_IMPLIZIT_START)
        defaultThreadPool.start(); // NOTE: different to ThreadPool, but this
                                   // should not really needed!
#endif

        BOOST_CHECK(defaultThreadPool.is_idle());
        BOOST_CHECK(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE(
            "defaultThreadPool.size: " << defaultThreadPool.size());
        defaultThreadPool.execute(new TestTask("Started ...", result));
        BOOST_CHECK(!defaultThreadPool.is_idle());

        unsigned i = 20;
        do {
            if (i > 5) {
                unsigned delay = std::rand() % 100;
                defaultThreadPool.execute(
                    new TestTask("Running ...", result, delay));
                Thread::sleep(i / 2); // ms
            }
            BOOST_TEST_MESSAGE("defaultThreadPool.queue_length: "
                << defaultThreadPool.queue_length());
        } while (--i > 0);

        do {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
            Thread::sleep(100); // ms
        } while (!defaultThreadPool.is_idle());
        BOOST_CHECK(defaultThreadPool.is_idle());
        BOOST_CHECK(!defaultThreadPool.is_busy());

        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST(TestTask::task_count() == 0UL);

        // NOTE: implicit done: defaultThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "All task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 16UL, "All task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolInterface_test)
{
    using namespace Agentpp;
    result_queue_t result;
    {
        QueuedThreadPool emptyThreadPool(
            0UL, 0x20000); // NOTE: without any worker thread! CK
        // NOTE: implicit done: emptyThreadPool.start();
        BOOST_TEST(emptyThreadPool.size() == 0UL);

#if !defined(USE_AGENTPP) && defined(USE_IMPLIZIT_START)
        BOOST_CHECK(emptyThreadPool.is_idle());
        emptyThreadPool.stop();
#endif

        emptyThreadPool.set_stack_size(
            0x20000); // NOTE: this change the queue thread only! CK
        BOOST_CHECK(emptyThreadPool.stack_size() == 0x20000);

        BOOST_TEST_MESSAGE("emptyThreadPool.size: " << emptyThreadPool.size());
        emptyThreadPool.execute(new TestTask("Starting ...", result));

        emptyThreadPool.start();

        BOOST_CHECK(!emptyThreadPool.is_idle());

        size_t i = 10;
        do {
            if (i > 5) {
                emptyThreadPool.execute(new TestTask("Running ...", result));
            }
            BOOST_TEST_MESSAGE("emptyThreadPool.queue_length: "
                << emptyThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        // FIXME: BOOST_CHECK(!emptyThreadPool.is_idle());
        BOOST_TEST_MESSAGE("outstanding tasks: " << TestTask::task_count());
        BOOST_TEST(TestTask::task_count() == 6UL);

        // NOTE: implicit done: emptyThreadPool.terminate();
    }
    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(TestTask::run_count() == 0UL, "NO task has to be executed!");
    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(QueuedThreadPoolIndependency_test)
{
    using namespace Agentpp;

    result_queue_t result;
    QueuedThreadPool firstThreadPool(1);
    BOOST_TEST(firstThreadPool.size() == 1UL);

    BOOST_TEST_MESSAGE("firstThreadPool.size: " << firstThreadPool.size());
    firstThreadPool.execute(new TestTask("Starting ...", result));

#if !defined(USE_IMPLIZIT_START)
    firstThreadPool.start();
    BOOST_CHECK(!firstThreadPool.is_idle());
#endif

    Thread::sleep(50); // ms
    BOOST_CHECK(firstThreadPool.is_idle());
    size_t n = 1;

    {
        QueuedThreadPool secondThreadPool(4);
        BOOST_TEST_MESSAGE(
            "secondThreadPool.size: " << secondThreadPool.size());

#if !defined(USE_IMPLIZIT_START)
        secondThreadPool.start();
#endif

        BOOST_CHECK(secondThreadPool.is_idle());

        secondThreadPool.execute(new TestTask("Starting ...", result));
        Thread::sleep(50); // ms
        BOOST_CHECK(secondThreadPool.is_idle());
        n++;

        secondThreadPool.terminate();
        secondThreadPool.execute(new TestTask("After terminate ...", result));
        // NO! n++; //XXX
        Thread::sleep(10); // ms

        size_t i = 10;
        do {
            if (i > 5) {
                secondThreadPool.execute(new TestTask("Queuing ...", result));
                // NO! n++; //XXX
            }
            BOOST_TEST_MESSAGE("secondThreadPool.queue_length: "
                << secondThreadPool.queue_length());
            Thread::sleep(10); // ms
        } while (--i > 0);

        if (!secondThreadPool.is_idle()) {
            BOOST_TEST_MESSAGE(
                "outstanding tasks: " << TestTask::task_count());
        }

        BOOST_TEST(TestTask::run_count() == n);
    }

    firstThreadPool.execute(new TestTask("Stopping ...", result));
    BOOST_TEST_MESSAGE(
        "firstThreadPool.queue_length: " << firstThreadPool.queue_length());
    Thread::sleep(50); // ms
    BOOST_CHECK(firstThreadPool.is_idle());
    firstThreadPool.terminate();

    BOOST_TEST(TestTask::task_count() == 0UL, "ALL task has to be deleted!");
    BOOST_TEST(
        TestTask::run_count() == (n + 1UL), "All task has to be executed!");

    TestTask::reset_counter();
}

BOOST_AUTO_TEST_CASE(Synchronized_test)
{
    using namespace Agentpp;
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
    using namespace Agentpp;
    Synchronized sync;
    {
        Lock l(sync);
        BOOST_TEST(sync.trylock() == Synchronized::OWNED);
    }
    BOOST_TEST(!sync.unlock(), "second unlock() returns no error");
}

BOOST_AUTO_TEST_CASE(SyncDeadlock_test)
{
    using namespace Agentpp;
    Synchronized sync;
    try {
        Lock l(sync);
        BOOST_TEST(!sync.lock());
    } catch (std::exception& e) {
        BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);
        BOOST_TEST_MESSAGE(e.what());
        BOOST_TEST(false);
    }
    BOOST_TEST(sync.lock());
    BOOST_TEST(sync.unlock());
}

BOOST_AUTO_TEST_CASE(SyncWait_test)
{
    using namespace Agentpp;
    Synchronized sync;
    {
        Lock l(sync);
        BOOST_TEST(!sync.wait(42), "no timeout occurred on wait!");
    }
}

BOOST_AUTO_TEST_CASE(ThreadSleep_test)
{
    using namespace Agentpp;

    Stopwatch sw;
    Thread::sleep(2000); // ms

    BOOST_TEST(sw.elapsed() >= ms(2000));
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << sw.elapsed());
}


struct wait_data {
    typedef boost::mutex lockable_type;
    typedef boost::unique_lock<lockable_type> scoped_lock;

    bool flag;
    lockable_type mtx;
    boost::condition_variable cond;

    wait_data()
        : flag(false)
    {}

    // NOTE: return false if condition waiting for is not true! CK
    bool predicate() { return flag; }

    void wait()
    {
        scoped_lock l(mtx);
        while (!predicate()) {
            cond.wait(l);
        }
    }

    template <typename Duration> bool timed_wait(Duration d)
    {
        scoped_lock l(mtx);
        while (!predicate()) {
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                return false;
            }
        }
        return true; // OK
    }

    void signal()
    {
        scoped_lock l(mtx);
        flag = true;
        cond.notify_all();
    }
};


typedef boost::testable_mutex<boost::mutex> mutex_type;

void lock_mutexes_slowly(
    mutex_type* m1, mutex_type* m2, wait_data* locked, wait_data* quit)
{
    boost::lock_guard<mutex_type> l1(*m1);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
    boost::lock_guard<mutex_type> l2(*m2);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    locked->signal();
    quit->wait();
}


void lock_pair(mutex_type* m1, mutex_type* m2)
{
    boost::lock(*m1, *m2);
    boost::unique_lock<mutex_type> l1(*m1, boost::adopt_lock),
        l2(*m2, boost::adopt_lock);
    BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION);

    BOOST_CHECK(l1.owns_lock());
    BOOST_CHECK(l2.owns_lock());
}


BOOST_AUTO_TEST_CASE(test_lock_two_other_thread_locks_in_order)
{
    mutex_type m1, m2;
    wait_data locked;
    wait_data release;

    boost::thread t(lock_mutexes_slowly, &m1, &m2, &locked, &release);

    boost::thread t2(lock_pair, &m1, &m2);
    BOOST_CHECK(locked.timed_wait(boost::chrono::seconds(1)));

    release.signal();

    BOOST_CHECK(t2.try_join_for(boost::chrono::seconds(1)));
    t2.join(); // just in case of timeout! CK

    t.join();
}


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

        error = ::boost::unit_test::unit_test_main(init_func, argc, argv);

        if (--loops <= 0) {
            break;
        }
        for (int i = 0; i < argc; ++i) {
            strcpy(argv[i], args[i].c_str());
            std::cout << i << ": " << argv[i] << std::endl;
        }
        std::cout << "loops left: " << loops << std::endl;
    } while (!error);

    return error;
}

