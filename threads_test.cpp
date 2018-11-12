//
// test program for agentpp threadpool
//
// astyle --style=kr thread*.{cpp,hpp}
// clang-format -style=file -i thread*.{cpp,hpp}
//

#include "simple_stopwatch.hpp"

#ifdef USE_AGENTPP
#include "agent_pp/threads.h" // ThreadPool, QueuedThreadPool
#else
#include "threadpool.hpp" // ThreadPool, QueuedThreadPool
#endif

#if (defined(_MSVC_LANG) && __has_include(<atomic>))                          \
    || (defined(__cplusplus) && (__cplusplus >= 201103L))
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#include <atomic>
typedef std::atomic_size_t test_counter_t;
#else
#include <boost/atomic.hpp>
typedef boost::atomic_size_t test_counter_t;
#endif

#define BOOST_TEST_MODULE Threads
#define BOOST_TEST_NO_MAIN
#include <boost/functional/hash.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/test/included/unit_test.hpp>

#include <iostream>
#include <string>
#include <vector>


typedef boost::lockfree::queue<size_t, boost::lockfree::capacity<20> >
    result_queue_t;

class TestTask : public Agentpp::Runnable {
public:
    explicit TestTask(
        const std::string& msg, result_queue_t& rslt, size_t ms_delay = 11)
        : text(msg)
        , result(rslt)
        , delay(ms_delay)
    {
        Agentpp::Lock l(lock);
        ++counter;
    }

    virtual ~TestTask()
    {
        Agentpp::Lock l(lock);
        --counter;
    }

    virtual void run()
    {
        Agentpp::Thread::sleep(delay); // ms

        Agentpp::Lock l(lock);
        // TODO BOOST_TEST_MESSAGE(BOOST_CURRENT_FUNCTION << " called with: "
        // << text);
        size_t hash = boost::hash_value(text);
        result.push(hash);
        ++run_cnt;
    }

    static size_t run_count()
    {
        Agentpp::Lock l(lock);
        return run_cnt;
    }
    static size_t task_count()
    {
        Agentpp::Lock l(lock);
        return counter;
    }
    static void reset_counter()
    {
        Agentpp::Lock l(lock);
        counter = 0;
        run_cnt = 0;
    }

protected:
    static test_counter_t run_cnt;
    static test_counter_t counter;
    static Agentpp::Synchronized lock;

private:
    const std::string text;
    result_queue_t& result;
    size_t delay;
};

Agentpp::Synchronized TestTask::lock;
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
        size_t i = 4;
        do {
            size_t delay = std::rand() % 100;
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

        size_t i = 20;
        do {
            if (i > 5) {
                size_t delay = std::rand() % 100;
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
        //FIXME: prevent deadlock! Lock l(sync);
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

BOOST_AUTO_TEST_CASE(SyncWait_test)
{
    using namespace Agentpp;
    Synchronized sync;
    {
        Lock l(sync);
        BOOST_TEST(sync.wait(42), "no timeout occurred on wait!");
    }
}

BOOST_AUTO_TEST_CASE(SleepThread_test)
{
    using namespace Agentpp;

    Stopwatch sw;
    Thread::sleep(2000); // ms

    BOOST_TEST(sw.elapsed() >= ms(2000));
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

/***
 * OUTPUT:

MINGW64 /e/workspace/cpp/threadpool $ make test
g++ -g -O -Wextra --std=c++98 -Wno-unused-parameter -DNDEBUG
-I/opt/local/include   -c -o threads_test.o threads_test.cpp g++ -g -O -Wextra
--std=c++98 -Wno-unused-parameter -DNDEBUG -I/opt/local/include
threads_test.o threadpool.o -o threads_test
./threads_test -l message ## --random

Running 4 test cases...
threadPool.size: 4
virtual void TestTask::run() called with: ThreadPool is running!
virtual void TestTask::run() called with: Under full load now!
virtual void TestTask::run() called with: Generate some load.
virtual void TestTask::run() called with: Hallo world!
outstanding tasks: 2
virtual void TestTask::run() called with: Good by!
outstanding tasks: 0

queuedThreadPool.size: 1
virtual void TestTask::run() called with: 1 Hi again.
queuedThreadPool.queue_length: 8
virtual void TestTask::run() called with: 2 Queuing starts.
virtual void TestTask::run() called with: 3 Under full load!
virtual void TestTask::run() called with: 4 Queuing ...
virtual void TestTask::run() called with: 5 Queuing ...
virtual void TestTask::run() called with: 6 Queuing ...
virtual void TestTask::run() called with: 7 Queuing ...
virtual void TestTask::run() called with: 8 Queuing ...
virtual void TestTask::run() called with: 9 Queuing ...
outstanding tasks: 0
NOTE: checking the order of execution

emptyThreadPool.size: 4
defaultThreadPool.queue_length: 0
virtual void TestTask::run() called with: Started ...
virtual void TestTask::run() called with: Running ...
defaultThreadPool.queue_length: 0
defaultThreadPool.queue_length: 0
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
defaultThreadPool.queue_length: 0
defaultThreadPool.queue_length: 0
defaultThreadPool.queue_length: 0
defaultThreadPool.queue_length: 0
defaultThreadPool.queue_length: 1
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
defaultThreadPool.queue_length: 1
defaultThreadPool.queue_length: 1
defaultThreadPool.queue_length: 1
defaultThreadPool.queue_length: 2
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
defaultThreadPool.queue_length: 2
outstanding tasks: 6

virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
virtual void TestTask::run() called with: Running ...
outstanding tasks: 0

emptyThreadPool.size: 0
emptyThreadPool.queue_length: 2
emptyThreadPool.queue_length: 3
emptyThreadPool.queue_length: 4
emptyThreadPool.queue_length: 5
emptyThreadPool.queue_length: 6
emptyThreadPool.queue_length: 6
emptyThreadPool.queue_length: 6
emptyThreadPool.queue_length: 6
emptyThreadPool.queue_length: 6
emptyThreadPool.queue_length: 6
outstanding tasks: 6

*** No errors detected

MINGW64 /e/workspace/cpp/threadpool $

 ***/
