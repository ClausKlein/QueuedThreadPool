/*_############################################################################
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
-DAGENTPP_USE_THREAD_POOL agent++/include/agent_pp/threads.h > threadpool.hpp
  _##
  _##########################################################################*/

#ifndef agent_pp_threadpool_hpp_
#define agent_pp_threadpool_hpp_

#define NO_LOGGING

#ifdef __INTEGRITY
#    include <integrity.h>
#endif

#ifdef _WIN32
#    define HAVE_STRUCT_TIMESPEC
#else
#    include <unistd.h> // _POSIX_MONOTONIC_CLOCK _POSIX_TIMEOUTS _POSIX_TIMERS _POSIX_THREADS ...
#endif

#include <iostream>
#include <list>
#include <pthread.h>
#include <queue>
#include <time.h>
#include <vector>

#include <boost/current_function.hpp>
#include <boost/noncopyable.hpp>

// NOTE: do not change! CK
#define AGENTPP_USE_IMPLIZIT_START

#define AGENTPP_DEFAULT_STACKSIZE 0x10000UL
#define AGENTPP_OPAQUE_PTHREAD_T void*
#define AGENTX_DEFAULT_PRIORITY 32
#define AGENTX_DEFAULT_THREAD_NAME "ThreadPool::Thread"
#define AGENTPP_DECL

#if !defined(NO_LOGGING) && !defined(NDEBUG)
#    define LOG_BEGIN(x, y) std::cerr << BOOST_CURRENT_FUNCTION << ": "
#    define LOG(x) std::cerr << x << ' '
#    define LOG_END std::cerr << std::endl
#else
#    define LOG_BEGIN(x, y)
#    define LOG(x)
#    define LOG_END
#endif

/*
 * Define a macro that can be used for diagnostic output from examples.
 * When compiled -DTRACE_VERBOSE, it results in writing with the
 * specified argument to std::cout. When DEBUG is not defined, it expands
 * to nothing.
 */
#ifdef TRACE_VERBOSE
#    define DTRACE(arg) \
        std::cerr << BOOST_CURRENT_FUNCTION << ": " arg << std::endl
#else
#    define DTRACE(arg)
#endif

namespace AgentppCK
{

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

class AGENTPP_DECL Runnable {

public:
    Runnable() { }
    virtual ~Runnable() { }

    /**
     * When an object implementing interface Runnable is used to
     * create a thread, starting the thread causes the object's run
     * method to be called in that separately executing thread.
     */
    virtual void run() = 0;
};

/**
 * The Synchronized class implements services for synchronizing
 * access between different threads.
 *
 * @author Frank Fock
 * @version 4.0
 */
class AGENTPP_DECL Synchronized : private boost::noncopyable {
public:
    enum TryLockResult { LOCKED = 1, BUSY = 0, OWNED = -1 };

    Synchronized();
    ~Synchronized();

    /**
     * Causes current thread to wait until another thread
     * invokes the notify() method or the notifyAll()
     * method for this object.
     */
    void wait();

    /**
     * Causes current thread to wait until either another
     * thread invokes the notify() method or the notifyAll()
     * method for this object, or a specified amount of time
     * has elapsed.
     *
     * @param timeout
     *    timeout in milliseconds.
     * @param
     *    return TRUE if timeout occured, FALSE otherwise.
     */
    bool wait(unsigned long timeout);

    /**
     * Wakes up a single thread that is waiting on this
     * object's monitor.
     */
    void notify();
    /**
     * Wakes up all threads that are waiting on this object's
     * monitor.
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

#if defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS > 0
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
#endif

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

    /**
     * Leave a critical section. If this thread called lock or trylock
     * more than once successfully, this call will nevertheless release
     * the lock (non-recursive locking).
     * @return
     *    TRUE if the unlock succeeded, FALSE if there was no lock
     *    to unlock.
     */
    bool unlock();

private:
#ifndef NO_LOGGING
    static int next_id;
    int id;
#endif

#ifndef _WIN32
    int cond_timed_wait(const timespec*);
#endif

    pthread_cond_t cond;
    pthread_mutex_t monitor;
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
     * thread invokes the notify() method or the notifyAll()
     * method for this object, or a specified amount of time
     * has elapsed.
     *
     * @param timeout
     *    timeout in milliseconds.
     */
    void wait(long timeout) // TODO: why NOT return bool? CK
    {
        if (timeout < 0) {
            (void)sync.wait();
        } else {
            (void)sync.wait(timeout);
        }
    }

    /**
     * Wakes up a single thread that is waiting on this
     * object's monitor.
     */
    void notify() { sync.notify(); }

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

    friend class Synchronized;
    friend void* thread_starter(void*);

public:
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
    ~Thread() BOOST_OVERRIDE;

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
    void run() BOOST_OVERRIDE;

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
    void start();

    /**
     * Before calling the start method this method can be used
     * to change the stack size of the thread.
     *
     * @param stackSize
     *    the thread's stack size in bytes.
     */
    void set_stack_size(size_t s) { stackSize = s; }

    /**
     * Check whether thread is alive.
     *
     * @return
     *    Returns TRUE if the thread is running; otherwise FALSE.
     */
    bool is_alive() const { return (status == RUNNING); }

    /**
     * Clone this thread. This method must not be called on
     * running threads.
     */
    Thread* clone() { return new Thread(get_runnable()); }

private:
    Runnable* runnable;
    ThreadStatus status;
    size_t stackSize;
    pthread_t tid;
    static ThreadList threadList;
    static void nsleep(time_t secs, long nanos);
};

/**
 * The ThreadList class implements a singleton class that holds
 * a list of all currently running Threads.
 *
 * @author Frank Fock
 * @version 3.5
 */
class AGENTPP_DECL ThreadList : public Synchronized {
public:
    ThreadList() { }
    ~ThreadList() { list.clear(); /* do no delete threads */ }

    void add(Thread* t)
    {
        Lock l(*this);
        list.push_back(t);
    }
    void remove(Thread* t)
    {
        Lock l(*this);
        list.remove(t);
    }
    size_t size()
    {
        Lock l(*this);
        return list.size();
    }

    Thread* last()
    {
        Lock l(*this);
        Thread* t = list.back();
        return t;
    }

private:
    std::list<Thread*> list;
};

class TaskManager;

/**
 * The ThreadPool class provides a pool of threads that can be
 * used to perform an arbitrary number of tasks.
 *
 * @author Frank Fock
 * @version 3.5.19
 */
class AGENTPP_DECL ThreadPool : public Synchronized {

    size_t stackSize;

protected:
    std::vector<TaskManager*> taskList;

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
     * @param stackSize
     *    the stack size for each thread.
     */
    ThreadPool(size_t size, size_t stackSize);

    /**
     * Destructor will wait for termination of all threads.
     */
    virtual ~ThreadPool();

    /**
     * Execute a task. The task will be deleted after call of
     * its run() method.
     */
    virtual void execute(Runnable*);

    /**
     * Check whether the ThreadPool is idle or not.
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    executing any task.
     */
    virtual bool is_idle();

    /**
     * Check whether the ThreadPool is busy (i.e., all threads are
     * running a task) or not.
     *
     * @return
     *    TRUE if all of the threads in the pool is currently
     *    executing any task.
     */
    virtual bool is_busy();

    /**
     * Get the size of the thread pool.
     * @return
     *    the number of threads in the pool.
     */
    size_t size() const { return taskList.size(); }

    /**
     * Get the stack size.
     *
     * @return
     *   the stack size of each thread in this thread pool.
     */
    size_t get_stack_size() const { return stackSize; }

    /**
     * Notifies the thread pool about an idle thread (synchronized).
     */
    virtual void idle_notification();

    /**
     * Gracefully stops all running task managers after their current
     * task execution. The ThreadPool cannot be used thereafter and should
     * be destroyed. This call blocks until all threads are stopped.
     */
    void terminate();
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
class AGENTPP_DECL QueuedThreadPool : public ThreadPool, public Runnable {

    Thread thread;

    std::queue<Runnable*> queue;
    volatile bool go;

public:
    /**
     * Create a ThreadPool with a given number of threads.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 4 threads.
     */
    explicit QueuedThreadPool(size_t size = 1);

    /**
     * Create a ThreadPool with a given number of threads and
     * stack size.
     *
     * @param size
     *    the number of threads started for performing tasks.
     *    The default value is 4 threads.
     * @param stackSize
     *    the stack size for each thread.
     */
    QueuedThreadPool(size_t size, size_t stackSize);

    /**
     * Destructor will wait for termination of all threads.
     */
    ~QueuedThreadPool() BOOST_OVERRIDE;

    /**
     * Execute a task. The task will be deleted after call of
     * its run() method.
     */
    void execute(Runnable*) BOOST_OVERRIDE;

    /**
     * Gets the current number of queued tasks.
     *
     * @return
     *    the number of tasks that are currently queued.
     */
    size_t queue_length()
    {
        Lock l(thread);
        return queue.size();
    }

    /**
     * Check whether QueuedThreadPool is idle or not.
     *
     * @return
     *    TRUE if non of the threads in the pool is currently
     *    executing any task and the queue is emtpy().
     */
    bool is_idle() BOOST_OVERRIDE;

    /**
     * Check whether the ThreadPool is busy
     * (i.e., (!queue.empty() || ThreadPool::is_busy())
     *
     * @return
     *    TRUE if all of the threads in the pool is currently
     *    executing any task or the queue is not empty.
     */
    bool is_busy() BOOST_OVERRIDE;

    /**
     * Stop queue processing.
     */
    void stop();

    /**
     * Notifies the thread pool about an idle thread.
     */
    void idle_notification() BOOST_OVERRIDE;

private:
    /**
     * Runs the queue processing loop.
     */
    void run() BOOST_OVERRIDE;

    /**
     * @note asserted to be called with lock! CK
     **/
    bool assign(Runnable* task, bool withQueuing = true);

    /**
     * @note asserted to be called with lock! CK
     **/
    bool is_stopped() { return !go; }

    void EmptyQueue();
};

/**
 * The TaskManager class controls the execution of tasks on
 * a Thread of a ThreadPool.
 *
 * @author Frank Fock
 * @version 3.5.19
 */
class AGENTPP_DECL TaskManager : public Synchronized, public Runnable {
    friend class ThreadPool;

public:
    /**
     * Create a TaskManager and insert the created thread
     * into the given ThreadPool.
     *
     * @param threadPool
     *    a pointer to a ThreadPool instance.
     * @param stackSize
     *    the stack size for the managed thread.
     */
    TaskManager(ThreadPool*, size_t stackSize = AGENTPP_DEFAULT_STACKSIZE);

    /**
     * Destructor will wait for thread to terminate.
     */
    ~TaskManager() BOOST_OVERRIDE;

    /**
     * Check whether this thread is idle or not.
     *
     * @return
     *   TRUE if the thread managed by this TaskManager does
     *   not currently execute any task; FALSE otherwise.
     */
    bool is_idle();

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
     */
    TaskManager* clone()
    {
        return new TaskManager(
            new ThreadPool(threadPool->size(), threadPool->get_stack_size()));
    }

private:
    Thread thread;
    ThreadPool* threadPool;
    Runnable* task;
    volatile bool go;

    /**
     * Start thread execution.
     */
    void start() { thread.start(); }
    /**
     * Stop thread execution after having finished current task.
     */
    void stop()
    {
        Lock l(*this);
        go = false;
    }
    void run() BOOST_OVERRIDE;
};

} // namespace AgentppCK

#endif
