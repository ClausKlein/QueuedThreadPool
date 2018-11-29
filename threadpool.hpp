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
-DAGENTPP_USE_THREAD_POOL -DNO_FAST_MUTEXES -DAGENTPP_PTHREAD_RECURSIVE
include/agent_pp/threads.h > threadpool.hpp
clang-format -i -style=file threadpool.{cpp,hpp}
  _##
  _##########################################################################*/

#ifndef agent_pp_threadpool_hpp_
#define agent_pp_threadpool_hpp_

#ifdef __INTEGRITY
#include <integrity.h>
#include <time.h>
#endif

#include <list>
#include <queue>
#include <vector>

#define BOOST_THREAD_VERSION 4
#define BOOST_CHRONO_VERSION 2

#include <boost/atomic.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_only.hpp>

#undef AGENTPP_QUEUED_THREAD_POOL_USE_ASSIGN

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
    Runnable() {}
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
    ~Synchronized();

    /**
     * Causes current thread to wait until another thread
     * invokes the notify() method or the notifyAll()
     * method for this object.
     *
     * @note asserted to be called with lock! CK
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
    bool try_lock() { return trylock(); };

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
    bool is_locked_by_this_thread() const
    {
        return boost::this_thread::get_id() == tid_;
    }
    bool is_locked() const { return !(boost::thread::id() == tid_); }

    // NOTE: the type of the wrapped lockable
    typedef boost::mutex lockable_type;
    lockable_type mutex;
    typedef boost::unique_lock<lockable_type> scoped_lock;

    boost::condition_variable cond;
    volatile bool signal;
    boost::atomic<boost::thread::id> tid_;
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
    void wait(long timeout)
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
    void notify() { sync.notify(); }

    /**
     * Wakes up all threads that are waiting on this object's
     * cond.
     */
    void notify_all() { sync.notify_all(); }

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
    virtual Runnable* get_runnable();

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
    Thread* clone()
    {
        BOOST_ASSERT(status != RUNNING);
        return new Thread(get_runnable());
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

    static ThreadList threadList;
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
    size_t size() const { return list.size(); }
    Thread* last()
    {
        Lock(*this);
        return list.back();
    }

protected:
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

protected:
    std::vector<TaskManager*> taskList;
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
    size_t size() const { return taskList.size(); }

    /**
     * Get the stack size.
     *
     * @return
     *   the stack size of each thread in this thread pool.
     */
    size_t get_stack_size() const { return stackSize; }

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
class AGENTPP_DECL QueuedThreadPool : public ThreadPool, public Thread {

    std::queue<Runnable*> queue;
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
    void run() BOOST_OVERRIDE;

    /**
     * Stop queue processing (SYNCHRONIZED).
     */
    void stop();

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

private:
    /**
     * @note asserted to be called with lock! CK
     **/
    bool assign(Runnable* task, bool withQueuing = true);
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
    TaskManager(ThreadPool*, size_t stack_size = AGENTPP_DEFAULT_STACKSIZE);

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
    bool is_idle() { return (!task && thread.is_alive()); }

    /**
     * Start thread execution.
     */
    void start() { thread.start(); }

    /**
     * Stop thread execution after having finished current task.
     */
    void stop() { go = false; }

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

protected:
    void run() BOOST_OVERRIDE;

    Thread thread;
    ThreadPool* threadPool;
    Runnable* task;
    volatile bool go;
};

} // namespace Agentpp

#endif // agent_pp_threadpool_hpp_
