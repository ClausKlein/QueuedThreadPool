/*_############################################################################
  _##
  _##  AGENT++ 4.0 - threads.cpp
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
-DAGENTPP_USE_THREAD_POOL -DNO_FAST_MUTEXES
src/threads.cpp > threadpool.cpp
  _##
  _##########################################################################*/

#ifndef _MSC_VER

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <unistd.h> // _POSIX_THREADS ...

#include <pthread.h>

#endif

#include "threadpool.hpp"

#include <iostream>

#if !defined(_NO_LOGGING) && !defined(NDEBUG)
#define DEBUG
#include <boost/current_function.hpp>
#define LOG_BEGIN(x, y) std::cout << BOOST_CURRENT_FUNCTION << ": "
#define LOG(x) std::cout << x << ' '
#define LOG_END std::cout << std::endl
#else
#define LOG_BEGIN(x, y)
#define LOG(x)
#define LOG_END
#define _NO_LOGGING 1
#endif
/*
 * Define a macro that can be used for diagnostic output from examples. When
 * compiled -DDEBUG, it results in writing with the specified argument to
 * std::cout. When DEBUG is not defined, it expands to nothing.
 */
#ifdef DEBUG
#define DTRACE(arg) \
    std::cout << BOOST_CURRENT_FUNCTION << ": " << arg << std::endl
#else
#define DTRACE(arg)
#endif


namespace Agentpp
{

#ifndef _NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
unsigned int Synchronized::next_id  = 0;
#endif


/*--------------------- class Synchronized -------------------------*/

Synchronized::Synchronized()
    : signal(false)
    , isLocked(false)
    , id_(boost::thread::id())

#ifndef _NO_LOGGING
    , id(0)
#endif

{}

Synchronized::~Synchronized()
{
    if (is_locked()) {
        notify_all();
        signal = true;
        unlock();
        // NOTE: give other waiting threads a time window to unlock the mutex
        boost::this_thread::sleep_for(ms(50));
    }
}

void Synchronized::wait() { cond_timed_wait(0); }

int Synchronized::cond_timed_wait(const struct timespec* ts)
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    scoped_lock l(mutex, boost::adopt_lock);
    if (!ts) {
        while (!signal) {
            //=================================
            id_ = boost::thread::id();
            cond.wait(l);
            id_ = boost::this_thread::get_id();
            //=================================
        }
    } else {
        duration d = sec(ts->tv_sec) + ns(ts->tv_nsec);
        while (!signal) {
            //=================================
            id_ = boost::thread::id();
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                id_ = boost::this_thread::get_id();
                l.release();
                return -1;
            }
            //=================================
        }
    }

    signal = false;
    id_    = boost::this_thread::get_id();
    l.release();
    return true;
}

bool Synchronized::wait(unsigned long timeout)
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    scoped_lock l(mutex, boost::adopt_lock);
    duration d = ms(timeout);
    while (!signal) {
        //=================================
        id_ = boost::thread::id();
        if (cond.wait_for(l, d) == boost::cv_status::timeout) {
            id_ = boost::this_thread::get_id();
            l.release();
            return false;
        }
        //=================================
    }

    signal = false;
    l.release();
    return true;
}

void Synchronized::notify()
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    scoped_lock l(mutex, boost::adopt_lock);
    signal = true;
    cond.notify_one();
    l.release();
}

void Synchronized::notify_all()
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    scoped_lock l(mutex, boost::adopt_lock);
    signal = true;
    cond.notify_all();
    l.release();
}

bool Synchronized::lock()
{
    DTRACE("");

    if (is_locked_by_this_thread()) {

#ifdef DEBUG
        throw std::runtime_error("Synchronized::lock(): recursive used!");
#endif

        // TODO: This thread owns already the lock, but we do not like
        // recursive locking. Thus release it immediately and print a
        // warning!
        return false;
    }

    mutex.lock();
    isLocked = true;
    id_      = boost::this_thread::get_id();
    return true;
}


#if HAVE_TIMED_MUTEX
bool Synchronized::lock(unsigned long timeout)
{
    DTRACE(timeout);
    BOOST_ASSERT(!is_locked_by_this_thread());

    duration d = ms(timeout);
    if (!mutex.try_lock_for(d)) {
        return false;
    }

    isLocked = true;
    id_      = boost::this_thread::get_id();
    return true; // OK
}
#endif


bool Synchronized::unlock()
{
    DTRACE("");

    if (is_locked_by_this_thread()) {
        isLocked = false;
        id_      = boost::thread::id();
        mutex.unlock();
        return true;
    }

    return false;
}

Synchronized::TryLockResult Synchronized::trylock()
{
    DTRACE("");

    if (is_locked_by_this_thread()) {
        return OWNED; // true
    }

    scoped_lock l(mutex, boost::try_to_lock);
    if (l.owns_lock()) {
        isLocked = true;
        id_      = boost::this_thread::get_id();
        l.release();
        return LOCKED; // true
    }

    return BUSY; // false
}


#if defined BOOST_THREAD_PROVIDES_NESTED_LOCKS
bool Synchronized::is_locked_by_this_thread()
{
    return mutex.is_locked_by_this_thread();
}
#endif


/*------------------------ class Thread ----------------------------*/

ThreadList Thread::threadList;

void* thread_starter(void* t)
{
    Thread* thread = static_cast<Thread*>(t);
    Thread::threadList.add(thread);

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: started (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;

#if defined(__APPLE__) && defined(_DARWIN_C_SOURCE)
    pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#endif

    thread->get_runnable()->run();

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: ended (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;

    Thread::threadList.remove(thread);
    thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread()
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
    , tid(0)
{
    runnable = static_cast<Runnable*>(this);
}

Thread::Thread(Runnable* r)
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
    , tid(0)
{
    runnable = r;
}

void Thread::run()
{
    LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
    LOG("Thread: empty run method!");
    LOG_END;
}

Thread::~Thread()
{
    // TODO: check this! CK
    if (status != IDLE) {
        join();
        DTRACE("Thread joined");
    }
}

Runnable* Thread::get_runnable() { return runnable; }

void Thread::join()
{
    // TODO: check this! CK
    if (status != IDLE) {
        void* retstat;
        int err = pthread_join(tid, &retstat);
        if (err) {
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: join failed (error)");
            LOG(err);
            LOG_END;
        }
        status = IDLE;
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 4);
        LOG("Thread: joined thread successfully (tid)");
        LOG((AGENTPP_OPAQUE_PTHREAD_T)tid);
        LOG_END;
    } else {
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
        LOG("Thread: thread not running (tid)");
        LOG((AGENTPP_OPAQUE_PTHREAD_T)tid);
        LOG_END;
    }
}

void Thread::start()
{
    // TODO: check this! CK
    if (status == IDLE) {
        int policy = 0;
        struct sched_param param;
        pthread_getschedparam(pthread_self(), &policy, &param);
        param.sched_priority = AGENTX_DEFAULT_PRIORITY;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, policy);
        pthread_attr_setschedparam(&attr, &param);

#if defined(__linux__) && defined(_GNU_SOURCE)
        pthread_attr_setthreadname_np(&attr, AGENTX_DEFAULT_THREAD_NAME);
#elif defined(__INTEGRITY)
        pthread_attr_setthreadname(&attr, AGENTX_DEFAULT_THREAD_NAME);
#elif defined(__APPLE__)
// NOTE: must be set from within the thread (can't specify thread ID)
// XXX pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#endif

        pthread_attr_setstacksize(&attr, stackSize);
        int err = pthread_create(&tid, &attr, thread_starter, this);
        if (err) {
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: cannot start thread (error)");
            LOG(err);
            LOG_END;

            DTRACE("Error: cannot start thread!");
            status = FINISHED; // NOTE: we are not started, see join()! CK
        } else {
            status = RUNNING;
        }
        pthread_attr_destroy(&attr);
    } else {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
        LOG("Thread: thread already running!");
        LOG_END;
    }
}

void Thread::sleep(long millis)
{
    // XXX nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000);
    boost::this_thread::sleep_for(ms(millis));
}

void Thread::sleep(long millis, long nanos)
{
    // XXX nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000 + nanos);
    boost::this_thread::sleep_for(ms(millis) + ns(nanos));
}

void Thread::nsleep(time_t secs, long nanos)
{
    // XXX time_t s = secs + nanos / 1000000000;
    // XXX long n   = nanos % 1000000000;
    boost::this_thread::sleep_for(sec(secs) + ns(nanos));
}


/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager(ThreadPool* tp, size_t stackSize)
    : thread(this)
{
    threadPool = tp;
    task       = 0;
    go         = true;
    thread.set_stack_size(stackSize);
    thread.start();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("TaskManager: thread started");
    LOG_END;
}

TaskManager::~TaskManager()
{
    {
        Lock l(*this);
        go = false;
        notify();
    }

    thread.join();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("TaskManager: thread joined");
    LOG_END;
}

void TaskManager::run()
{
    // TODO: Lock l(*this);
    lock();
    while (go) {
        if (task) {
            task->run(); // NOTE: executes the task
            delete task;
            task = 0;

            unlock(); // NOTE: prevent deadlock! CK
            //==============================
            // NOTE: may end in a direct call to set_task()
            // via QueuedThreadPool::run() => QueuedThreadPool::assign()
            threadPool->idle_notification();
            //==============================
            lock(); // NOTE: needed! CK
        } else {
            wait(); // NOTE: idle, wait until notify signal CK
        }
    }
    if (task) {
        delete task;
        task = 0;
        DTRACE("task deleted after stop()");
    }
    unlock();
}


// TODO: assert to be called with lock! CK
bool TaskManager::set_task(Runnable* t)
{
    Lock l(*this);
    if (!task) {
        task = t;
        notify();
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
        LOG("TaskManager: after notify");
        LOG_END;
        return true;
    } else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
        LOG("TaskManager: got already a task");
        LOG_END;
        return false;
    }
}


/*--------------------- class ThreadPool --------------------------*/

void ThreadPool::execute(Runnable* t)
{
    // TODO: Lock l(*this);
    lock();
    TaskManager* tm = 0;
    while (!tm) {
        for (std::vector<TaskManager*>::iterator cur = taskList.begin();
             cur != taskList.end(); ++cur) {
            tm = *cur;
            if (tm->is_idle()) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
                LOG("TaskManager: task manager found");
                LOG_END;

                unlock();
                //==============================
                if (tm->set_task(t)) {
                    return; // done
                } else {
                    tm = 0; // task could not be assigned
                }
                //==============================
                lock();
            }
            tm = 0;
        }
        if (!tm) {
            DTRACE("busy! Synchronized::wait()");
            wait(); // NOTE: (ms) for idle_notification ... CK
        }
    }
    unlock();
}


// TODO: assert to be called with lock! CK
void ThreadPool::idle_notification()
{
    Lock l(*this);
    notify();
}

/// return true if NONE of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_idle()
{
    // TODO: use Lock l(*this);
    lock();
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        if (!(*cur)->is_idle()) {
            unlock();
            return false;
        }
    }
    unlock();
    return true; // NOTE: all threads are idle
}

/// return true if ALL of the threads in the pool is currently executing
/// any task.
bool ThreadPool::is_busy()
{
    // TODO: use Lock l(*this);
    lock();
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        if ((*cur)->is_idle()) {
            unlock();
            return false;
        }
    }
    unlock();
    return true; // NOTE: all threads are busy
}

void ThreadPool::terminate()
{
    // TODO: use Lock l(*this);
    lock();
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        (*cur)->stop();
    }
    notify(); // NOTE: for wait() at execute()
    unlock();
}

ThreadPool::ThreadPool(size_t size)
    : stackSize(AGENTPP_DEFAULT_STACKSIZE)
{
    for (size_t i = 0; i < size; i++) {
        taskList.push_back(new TaskManager(this));
    }
}

ThreadPool::ThreadPool(size_t size, size_t stack_size)
    : stackSize(stack_size)
{
    for (size_t i = 0; i < size; i++) {
        taskList.push_back(new TaskManager(this, stackSize));
    }
}

ThreadPool::~ThreadPool()
{
    terminate();

    for (size_t i = 0; i < taskList.size(); i++) {
        delete taskList[i];
    }
}


/*--------------------- class QueuedThreadPool --------------------------*/

QueuedThreadPool::QueuedThreadPool(size_t size)
    : ThreadPool(size)
    , go(false)
{
#ifdef USE_IMPLIZIT_START
    go = true;

    start();
#endif
}

QueuedThreadPool::QueuedThreadPool(size_t size, size_t stack_size)
    : ThreadPool(size, stack_size)
    , go(false)
{
#ifdef USE_IMPLIZIT_START
    go = true;

    start();
#endif
}

QueuedThreadPool::~QueuedThreadPool()
{
    stop();

    join();
    DTRACE("thread joined");

    while (!queue.empty()) {
        Runnable* t = queue.front();
        queue.pop();
        if (t) {
            delete t;
            DTRACE("queue entry (task) deleted");
        }
    }

    ThreadPool::terminate();
}

// NOTE: asserted to be called with lock! CK
bool QueuedThreadPool::assign(Runnable* t, bool withQueuing)
{
    TaskManager* tm = 0;
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        tm = *cur;
        if (tm->is_idle()) {
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
            LOG("QueuedThreadPool::assign(IDLE):: task manager found");
            LOG_END;

            Thread::unlock();
            //==============================
            if (!tm->set_task(t)) {
                tm = 0;
                Thread::lock();
            } else {
                Thread::lock();
                DTRACE("task manager found");
                return true; // OK
            }
            //==============================
        }
        tm = 0;
    }

#ifndef AGENTPP_QUEUED_THREAD_POOL_USE_ASSIGN
    // NOTE: no idle thread found, push to queue if allowed! CK
    if (!tm && withQueuing) {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
        LOG("QueuedThreadPool::assign(BUSY): queue.add()");
        LOG_END;
        queue.push(t);

        DTRACE("busy! task queued; Thread::notify()");
        Thread::notify();

        return true;
    }
#endif

    return false;
}

void QueuedThreadPool::execute(Runnable* t)
{
    Thread::lock();

#ifdef AGENTPP_QUEUED_THREAD_POOL_USE_ASSIGN
    if (queue.empty()) {
        assign(t);
    } else
#endif

    {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
        LOG("QueuedThreadPool::execute() queue.push");
        LOG_END;
        queue.push(t);
        Thread::notify();
    }

    Thread::unlock();
}

void QueuedThreadPool::run()
{
    Thread::lock();

    go = true;
    while (go) {
        if (!queue.empty()) {
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
            LOG("queue.front");
            LOG_END;
            Runnable* t = queue.front();
            if (t) {
                if (assign(t, false)) { // NOTE: without queuing! CK
                    queue.pop();        // OK, now we pop this entry

                } else {
                    DTRACE("busy! Thread::sleep()");
                    // NOTE: Wait some ms to prevent notify() loops while busy!
                    // CK
                    Thread::sleep(rand() % 113); // ms
                }
            }
        }

        // NOTE: for idle_notification ... CK
        Thread::wait(); // ms
    }

    Thread::unlock();
}

size_t QueuedThreadPool::queue_length()
{
    Thread::lock();
    size_t length = queue.size();
    Thread::unlock();

    return length;
}

void QueuedThreadPool::idle_notification()
{
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("QueuedThreadPool::idle_notification");
    LOG_END;

    Thread::lock();
    Thread::notify();
    Thread::unlock();

    // TODO: why? CK
    ThreadPool::idle_notification();
}

bool QueuedThreadPool::is_idle()
{
    Thread::lock();
    bool result = is_alive() && queue.empty() && ThreadPool::is_idle();
    Thread::unlock();

    return result;
}

bool QueuedThreadPool::is_busy()
{
    Thread::lock();
    bool result = !queue.empty() || ThreadPool::is_busy();
    Thread::unlock();

    return result;
}

void QueuedThreadPool::stop()
{
    Thread::lock();
    go = false;
    Thread::notify();
    Thread::unlock();
}

} // namespace Agentpp
