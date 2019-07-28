/*_###########################################################################
  _##
  _##  AGENT++ 4.0 - threads.h
  _##
  _##  Copyright (C) 2000-2013  Frank Fock and Jochen Katz (agentpp.com)
  _##
  _##  Licensed under the Apache License, Version 2.0 (the "License");
  _##  you may not use this file except in compliance with the License.
  _##  You may obtain a copy of the License at
  _##
  _##      http://www.apache.org/licenses/LICENSE-2.0
  _##
  _##  Unless required by applicable law or agreed to in writing, software
  _##  distributed under the License is distributed on an "AS IS" BASIS,
  _##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  _##  See the License for the specific language governing permissions and
  _##  limitations under the License.
  _##
  _##  created and modified by claus.klein@arcormail.de
unifdef -U_WIN32THREADS -UWIN32 -DPOSIX_THREADS -DAGENTPP_NAMESPACE -D_THREADS
-DAGENTPP_USE_THREAD_POOL -DNO_FAST_MUTEXES -DAGENTPP_PTHREAD_RECURSIVE
include/agent_pp/threads.h > threadpool.hpp
clang-format -i -style=file threadpool.{cpp,hpp}
  _##
  _##  Note: The start/stop of QueuedThreadPool does not have a clear concept:
  _##   If not started, the QueuedThreadPool::run() terminates immediately! CK
  _#########################################################################*/

#ifndef agent_pp_threadpool_hpp_
#define agent_pp_threadpool_hpp_

#ifdef __INTEGRITY
#include <integrity.h>
#include <time.h>
#endif

#include <list>
#include <memory>
#include <queue>
#include <vector>

#if !defined BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_QUEUE_DEPRECATE_OLD
#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_VERSION 4
#define BOOST_CHRONO_VERSION 2

#include <boost/atomic.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_only.hpp>

#if 0
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/smart_ptr/make_shared.hpp>

namespace own {
template<typename T>
using unique_ptr = std::unique_ptr<T>;

template<typename T>
using make_unique = std::make_unique<T>;

template<typename T>
using shared_ptr = std::shared_ptr<T>;

template<typename T>
using make_shared = std::make_shared<T>;

template<typename T>
using enable_shared_from_this = std::enable_shared_from_this<T>;
}
#endif

// Do NOT change! CK
#undef AGENTPP_QUEUED_THREAD_POOL_USE_ASSIGN
#define AGENTPP_SET_TASK_USE_TRY_LOCK
#define AGENTPP_USE_IMPLIZIT_START

// This may be changed CK
#ifdef _DEBUG
#define DEBUG 1
#undef NDEBUG
#define AGENTPP_DEBUG
#endif

// ONLY for DEMO: be carefully! CK
#undef AGENTPP_USE_YIELD
#undef CREATE_RACE_CONDITION

#define AGENTPP_DEFAULT_STACKSIZE 0x10000UL
#define AGENTX_DEFAULT_PRIORITY 32
#define AGENTX_DEFAULT_THREAD_NAME "ThreadPool::Thread"

#ifndef AGENTPP_OPAQUE_PTHREAD_T
#define AGENTPP_OPAQUE_PTHREAD_T void*
#endif

#ifndef AGENTPP_DECL
#define AGENTPP_DECL
#endif

#ifndef BOOST_OVERRIDE
#define BOOST_OVERRIDE
#endif

namespace Agentpp
{

typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::seconds sec;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::microseconds us;
typedef boost::chrono::nanoseconds ns;


// NOTE: this prevents prevent slicing too! CK
class ClonableBase {
public:
    ClonableBase()          = default;
    virtual ~ClonableBase() = default;
    // FIXME virtual std::unique_ptr<ClonableBase> clone() const = 0;

    ClonableBase(const ClonableBase&) = delete;
    ClonableBase& operator=(const ClonableBase&) = delete;
    ClonableBase(ClonableBase&&)                 = delete;
    ClonableBase& operator=(ClonableBase&&) = delete;
};

/**
 * The Runnable interface should be implemented by any class whose
 * instances are intended to be executed by a thread. The class must
 * define a method of no arguments called run.
 *
 * This interface is designed to provide a common protocol for objects
 * that wish to execute code while they are active. For example,
 * Runnable is implemented by class Thread. Being active simply means
 * that a thread has been started and has not yet been stopped.
 *
 * In addition, Runnable provides the means for a class to be active
 * while not subclassing Thread. A class that implements Runnable can
 * run without subclassing Thread by instantiating a Thread instance and
 * passing itself in as the target. In most cases, the Runnable interface
 * should be used if you are only planning to override the run() method
 * and no other Thread methods. This is important because classes should
 * not be subclassed unless the programmer intends on modifying or
 * enhancing the fundamental behavior of the class.
 *
 * @author Frank Fock
 * @version 3.5
 */
class AGENTPP_DECL Runnable : public ClonableBase {

public:
    Runnable()
        : ClonableBase()
    {}
    virtual ~Runnable() {}

    /**
     * When an object implementing interface Runnable is used to
     * create a thread, starting the thread causes the object's run
     * method to be called in that separately executing thread.
     */
    virtual void run() = 0;
    void operator()() { run(); };
};

/**
 * The Synchronized class implements services for synchronizing
 * access between different threads.
 *
 * @author Frank Fock
 * @version 4.0
 *
 * @note: copy constructor of 'Synchronized' is implicitly deleted
 *       because field 'cond' has a deleted copy constructor! CK
 */
class AGENTPP_DECL Synchronized : private boost::noncopyable {
public:
    enum TryLockResult { LOCKED = 1, BUSY = 0, OWNED = -1 };

    Synchronized();
    virtual ~Synchronized();

    /**
     * Causes current thread to wait until another thread
     * invokes the notify() method or the notify_all()
     * method for this object.
     *
     * @note asserted to be called with lock! CK
     */
    void wait();

    /**
     * Causes current thread to wait until either another
     * thread invokes the notify() method or the notify_all()
     * method for this object, or a specified amount of time
     * has elapsed.
     *
     * @param timeout
     *    timeout in milliseconds.
     * @param
     *    return TRUE if timeout occurred, FALSE otherwise.
     *
     * @note asserted to be called with lock! CK
     */
    bool wait(unsigned long timeout);

    /**
     * Wakes up a single thread that is waiting on this
     * object's cond.
     *
     * @note asserted to be called with lock! CK
     */
    void notify();

    /**
     * Wakes up all threads that are waiting on this object's
     * cond.
     *
     * @note asserted to be called with lock! CK
     */
    void notify_all();

    /**
     * Enter a critical section. If this thread owned this
     * lock already, the call succeeds too (returns TRUE), but there
     * will not be recursive locking. Unlocking will always free the lock.
     *
     * @return
     *    TRUE if the attempt was successful, FALSE otherwise.
     */
    bool lock();

    /**
     * Try to enter a critical section. If this thread owned this
     * lock already, the call succeeds too (returns TRUE), but there
     * will not be recursive locking. Unlocking will always free the lock.
     *
     * @param timeout
     *    timeout in milliseconds. If timeout occurred FALSE is returned.
     * @return
     *    TRUE if the attempt was successful, FALSE otherwise.
     */
    bool lock(unsigned long timeout);

    /**
     * Try to enter a critical section. If this thread owned this
     * lock already, the call succeeds too (returns TRUE), but there
     * will not be recursive locking. Unlocking will always free the lock.
     *
     * @return
     *     LOCKED if there was no lock before and now the calling thread
     *     owns the lock; BUSY = if another thread owns the lock;
     *     OWNED if the lock is already owned by the calling thread.
     */
    TryLockResult trylock();

    /// to be a std::lockable too
    inline bool try_lock() { return trylock(); };

    /**
     * Leave a critical section. If this thread called lock or trylock
     * more than once successfully, this call will nevertheless release
     * the lock (non-recursive locking).
     *
     * @return
     *    TRUE if the unlock succeeded, FALSE if there was no lock
     *    to unlock.
     */
    bool unlock();

protected:
    // NOTE: the type of the wrapped lockable
    typedef boost::mutex lockable_type;
    lockable_type mutex;
    typedef boost::unique_lock<lockable_type> scoped_lock;
    boost::condition_variable cond;
    volatile bool signal;
    boost::atomic<boost::thread::id> tid_;

    inline bool is_locked_by_this_thread() const
    {
        return boost::this_thread::get_id() == tid_;
    }

    inline bool is_locked() const { return !(boost::thread::id() == tid_); }

    inline void wait_until_condition(
        scoped_lock& lk, boost::function<bool()> predicate)
    {
        while (!predicate()) {
            //=================================
            tid_ = boost::thread::id();
            cond.wait(lk); // forever
            tid_ = boost::this_thread::get_id();
            //=================================
        }
    }

    inline void wait_for_signal_if_needed(
        scoped_lock& lk, volatile bool& signal)
    {
        while (!signal) {
            //=================================
            tid_ = boost::thread::id();
            cond.wait(lk); // forever
            tid_ = boost::this_thread::get_id();
            //=================================
        }
    }
};

/**
 * The Lock class implements a synchronization object, that
 * when created enters the critical section of the given
 * Synchronized object and leaves it when the Lock object is
 * destroyed. The execution of the critical section can be
 * suspended by calling the wait function.
 *
 * @author Frank Fock
 * @version 3.5
 */
class AGENTPP_DECL Lock : private boost::noncopyable {
public:
    /**
     * Create a locking object for a Synchronized instance,
     * which will be locked by calling this constructor.
     *
     * @param sync
     *   a Synchronized instance.
     */
    explicit Lock(Synchronized& s)
        : sync(s)
    {
        sync.lock();
    }

    /**
     * The destructor will release the lock on the sync
     * object.
     */
    ~Lock() { sync.unlock(); }

    /**
     * Causes current thread to wait until either another
     * thread invokes the notify() method or the notify_all()
     * method for this object, or a specified amount of time
     * has elapsed.
     *
     * @param timeout
     *    timeout in milliseconds.
     */
    inline void wait(long timeout)
    {
        if (timeout < 0) {
            sync.wait();
        } else {
            sync.wait(timeout);
        }
    }

    /**
     * Wakes up a single thread that is waiting on this
     * object's cond.
     */
    inline void notify() { sync.notify(); }

    /**
     * Wakes up all threads that are waiting on this object's
     * cond.
     */
    inline void notify_all() { sync.notify_all(); }

private:
    Synchronized& sync;
};

class AGENTPP_DECL ThreadList;

/**
 * A thread is a thread of execution in a program.
 *
 * There are two ways to create a new thread of execution. One is to
 * declare a class to be a subclass of Thread. This subclass should
 * override the run method of class Thread. An instance of the subclass
 * can then be allocated and started.
 *
 * The other way to create a thread is to declare a class that
 * implements the Runnable interface. That class then implements the run
 * method. An instance of the class can then be allocated, passed as an
 * argument when creating Thread, and started.
 *
 * @author Frank Fock
 * @version 3.5.7
 */
class AGENTPP_DECL Thread : public Synchronized, public Runnable {

    enum ThreadStatus { IDLE, RUNNING, FINISHED };

public:
    static void* thread_starter(void*); // for access to ThreadList

    /**
     * Create a new thread.
     */
    Thread();

    /**
     * Create a new thread which will execute the given Runnable.
     *
     * @param runnable
     *    a Runnable subclass.
     */
    explicit Thread(Runnable* r);

    /**
     * Destroy thread. If thread is running or has been finished but
     * not joined yet, then join it.
     */
    virtual ~Thread();

    /**
     * Causes the currently executing thread to sleep (temporarily
     * cease execution) for the specified number of milliseconds.
     *
     * @param millis
     *    number of milliseconds to sleep.
     */
    static void sleep(long millis);

    /**
     * Causes the currently executing thread to sleep (cease
     * execution) for the specified number of milliseconds plus
     * the specified number of nanoseconds.
     *
     * @param millis
     *    the length of time to sleep in milliseconds.
     * @param nanos
     *    0-999999 additional nanoseconds to sleep.
     */
    static void sleep(long millis, long nanos);

    /**
     * If this thread was constructed using a separate Runnable
     * run object, then that Runnable object's run method is called;
     * otherwise, this method does nothing and returns.
     *
     * Subclasses of Thread should override this method.
     */
    virtual void run() BOOST_OVERRIDE;

    /**
     * Get the Runnable object used for thread execution.
     *
     * @return
     *    a Runnable instance which is either the Thread itself
     *    when created through the default constructor or the
     *    Runnable object given at creation time.
     */
    Runnable* get_runnable();

    /**
     * Waits for this thread to die.
     */
    void join();

    /**
     * Causes this thread to begin execution; the system calls the
     * run method of this thread.
     */
    virtual void start();

    /**
     * Before calling the start method this method can be used
     * to change the stack size of the thread.
     *
     * @param stackSize
     *    the thread's stack size in bytes.
     */
    inline void set_stack_size(size_t s) { stackSize = s; }

    /**
     * Check whether thread is alive.
     *
     * @return
     *    Returns TRUE if the thread is running; otherwise FALSE.
     */
    inline bool is_alive() const { return (status == RUNNING); }

    /**
     * Clone this thread. This method must not be called on
     * running threads.
     * see too:
     * http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
     * #c21-if-you-define-or-delete-any-default-operation-define-or-delete-them-all
     */
    std::unique_ptr<Thread> clone()
    {
        BOOST_ASSERT(status != RUNNING);
        return std::make_unique<Thread>(get_runnable());
    }

private:
    static void nsleep(time_t secs, long nanos);

    Runnable* runnable;
    ThreadStatus status;
    size_t stackSize;

#ifdef POSIX_THREADS
    pthread_t tid;
#else
    boost::thread tid;
#endif

    // XXX static ThreadList threadList;
};

#if 0
/**
 * The ThreadList class implements a singleton class that holds
 * a list of all currently running Threads.
 *
 * @author Frank Fock
 * @version 3.5
 */
class AGENTPP_DECL ThreadList : public Synchronized {
public:
    ThreadList() {}
    ~ThreadList() { list.clear(); /* does not delete threads */ }

    void add(Thread* t)
    {
        Lock(*this);
        list.push_back(t);
    }
    void remove(Thread* t)
    {
        Lock(*this);
        list.remove(t);
    }
    inline size_t size() const { return list.size(); }
    Thread* last()
    {
        Lock(*this);
        return list.back();
    }

protected:
    std::list<Thread*> list;
};
#endif

class TaskManager;

/**
 * The ThreadPool class provides a pool of threads that can be
 * used to perform an arbitrary number of tasks.
 *
 * @author Frank Fock
 * @version 3.5.19
 */
class AGENTPP_DECL ThreadPool
    : public Synchronized,
      public std::enable_shared_from_this<ThreadPool> {

protected:
    std::vector<std::unique_ptr<TaskManager> > taskList;
    size_t stackSize;

public:
    /**
     * Create a ThreadPool with a given number of threads.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 4 threads.
     */
    explicit ThreadPool(size_t size = 4);

    /**
     * Create a ThreadPool with a given number of threads and
     * stack size.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 4 threads.
     * @param stack_size
     *    the stack size for each thread.
     */
    ThreadPool(size_t size, size_t stack_size);

    /**
     * Destructor will wait for termination of all threads.
     */
    virtual ~ThreadPool();

    /**
     * Execute a task. The task will be deleted after call of
     * its run() method (SYNCHRONIZED).
     */
    virtual void execute(Runnable*);

    /**
     * Check whether the ThreadPool is idle or not (SYNCHRONIZED).
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    executing any task.
     */
    virtual bool is_idle();

    /**
     * Check whether the ThreadPool is busy (i.e., all threads are
     * running a task) or not (SYNCHRONIZED).
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    idle (not executing any task).
     */
    virtual bool is_busy();

    /**
     * Get the size of the thread pool.
     * @return
     *    the number of threads in the pool.
     */
    virtual size_t size() const { return taskList.size(); }

    /**
     * Get the stack size.
     *
     * @return
     *   the stack size of each thread in this thread pool.
     */
    inline size_t get_stack_size() const { return stackSize; }

    /**
     * Notifies the thread pool about an idle thread (SYNCHRONIZED).
     */
    virtual void idle_notification();

    /**
     * Gracefully stops all running task managers after their current task
     * execution. The ThreadPool cannot be used thereafter and should be
     * destroyed. This call blocks until all threads are stopped
     * (SYNCHRONIZED).
     */
    virtual void terminate();
};

/**
 * The QueuedThreadPool class provides a pool of threads that can be
 * used to perform an arbitrary number of tasks. If a task is added
 * and there is currently no idle thread available to perform the task,
 * then the task will be queued for later processing. Consequently,
 * the execute method never blocks (in contrast to ThreadPool).
 *
 * The QueuedThreadPool uses an extra Thread to process queued messages
 * asynchronously.
 *
 * @author Frank Fock
 * @version 3.5.18
 */
class AGENTPP_DECL QueuedThreadPool : public ThreadPool, public Thread {

    // XXX std::queue<Runnable*> queue;
    size_t _size;
    volatile bool go;

public:
    /**
     * Create a ThreadPool with a given number of threads.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 1 threads.
     */
    explicit QueuedThreadPool(size_t size = 1);

    /**
     * Create a ThreadPool with a given number of threads and
     * stack size.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 4 threads.
     * @param stack_size
     *    the stack size for each thread.
     */
    QueuedThreadPool(size_t size, size_t stack_size);

    /**
     * Destructor will wait for termination of all threads.
     */
    virtual ~QueuedThreadPool();

    /**
     * Execute a task. The task will be deleted after call of
     * its run() method.
     */
    virtual void execute(Runnable*) BOOST_OVERRIDE;

    /**
     * Gets the current number of queued tasks (SYNCHRONIZED).
     *
     * @return
     *    the number of tasks that are currently queued.
     */
    size_t queue_length();

    /**
     * Runs the queue processing loop (SYNCHRONIZED).
     */
    // XXX virtual void run() BOOST_OVERRIDE;

    /**
     * Notifies the thread pool about an idle thread (SYNCHRONIZED).
     */
    virtual void idle_notification() BOOST_OVERRIDE;

    /**
     * Check whether QueuedThreadPool is idle or not (SYNCHRONIZED).
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    executing any task and the queue is emtpy().
     */
    virtual bool is_idle() BOOST_OVERRIDE;

    /**
     * Check whether the ThreadPool is busy (i.e., all threads are
     * running a task) or not (SYNCHRONIZED).
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    idle (not executing any task).
     */
    virtual bool is_busy() BOOST_OVERRIDE;

    virtual size_t size() const BOOST_OVERRIDE { return _size; }

    virtual void terminate() BOOST_OVERRIDE;

protected:
    /**
     * Stop queue processing (SYNCHRONIZED).
     *
     * @note: the run() returns and the thread terminates too!
     */
    virtual void stop() BOOST_OVERRIDE;

    // XXX inline bool has_task() { return (!go || !queue.empty()); }

private:
    /**
     * @note asserted to be called with lock! CK
     **/
    // XXX bool assign(Runnable* task, bool withQueuing = true);

    std::unique_ptr<boost::basic_thread_pool> ea;
};

/**
 * The TaskManager class controls the execution of tasks on
 * a Thread of a ThreadPool.
 *
 * @author Frank Fock
 * @version 3.5.19
 */
class AGENTPP_DECL TaskManager : public Synchronized, public Runnable {
public:
    /**
     * Create a TaskManager and insert the created thread
     * into the given ThreadPool.
     *
     * @param threadPool
     *    a pointer to a ThreadPool instance.
     * @param stack_size
     *    the stack size for the managed thread.
     */
    // TODO TaskManager(std::shared_ptr<ThreadPool> tp,
    TaskManager(ThreadPool* tp, size_t stack_size = AGENTPP_DEFAULT_STACKSIZE);

    /**
     * Destructor will wait for thread to terminate.
     */
    virtual ~TaskManager();

    /**
     * Check whether this thread is idle or not.
     *
     * @return
     *   TRUE if the thread managed by this TaskManager does
     *   not currently execute any task and the associated thread is running;
     *   FALSE otherwise.
     */
    inline bool is_idle() const { return (!task && thread.is_alive()); }

    /**
     * Start thread execution.
     */
    virtual void start() BOOST_OVERRIDE { thread.start(); }

    /**
     * Stop thread execution after having finished current task.
     */
    virtual void stop() BOOST_OVERRIDE { go = false; }

    /**
     * Set the next task for execution. This will block until
     * current task has finished execution.
     *
     * @param task
     *   a Runnable instance.
     * @return
     *   TRUE if the task could be assigned successfully and
     *   FALSE if another thread has assigned a task concurrently.
     *   In the latter case, the task has not been assigned!
     */
    bool set_task(Runnable*);

    /**
     * Clone this TaskManager.
     * see too:
     * http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
     * #c21-if-you-define-or-delete-any-default-operation-define-or-delete-them-all
     * and
     * https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines-owning-memory.html
     */
    std::unique_ptr<TaskManager> clone()
    {
        return std::make_unique<TaskManager>( // FIXME: prevent memoryleek! CK
                                              // std::make_shared<ThreadPool>
            new ThreadPool(threadPool->size(), threadPool->get_stack_size()));
        // warning: initializing non-owner argument of type
        // 'Agentpp::ThreadPool *&&' with a newly created 'gsl::owner<>'
        // [cppcoreguidelines-owning-memory]
    }

protected:
    /**
     * Runs the task (SYNCHRONIZED).
     */
    virtual void run() BOOST_OVERRIDE;

    inline bool has_task() { return (!go || task); }

    Thread thread;
    // TODO std::shared_ptr<ThreadPool> threadPool;
    ThreadPool* threadPool;
    Runnable* task; // TODO: should be a std::unique_ptr<Runnable>
    volatile bool go;
};

} // namespace Agentpp

#endif // agent_pp_threadpool_hpp_
