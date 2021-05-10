/*_############################################################################
  _##
  _##  AGENT++ 4.5 - threads.cpp
  _##
  _##  Copyright (C) 2000-2021  Frank Fock and Jochen Katz (agentpp.com)
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
  _## generate by Claus Klein with:
  _## unifdef -U_WIN32THREADS -UWIN32 -DPOSIX_THREADS -DAGENTPP_NAMESPACE
  -D_THREADS -DAGENTPP_USE_THREAD_POOL
  /Users/clausklein/Downloads/agentpp/agent++-4.5.0/src/threads.cpp >
  threads.cpp
  _##########################################################################*/

#include "agent_pp/threads.h"

#include <agent_pp/mib.h>
#include <agent_pp/mib_entry.h>
// NO! #include <snmp_pp/log.h>

#include <cassert>
#include <cerrno>

#undef _NO_LOGGING
#ifndef _NO_LOGGING
#    define _NO_LOGGING 1
#    undef LOG_BEGIN
#    define LOG_BEGIN(x, y)
#    undef LOG
#    define LOG(x)
#    undef LOG_END
#    define LOG_END
#endif

namespace Agentpp
{

#ifndef _NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif

#ifndef AGENTPP_SYNCHRONIZED_UNLOCK_RETRIES
#    define AGENTPP_SYNCHRONIZED_UNLOCK_RETRIES 1000
#endif

Synchronized ThreadManager::global_lock;

/**
 * Default constructor
 */
ThreadManager::ThreadManager() { }

/**
 * Destructor
 */
ThreadManager::~ThreadManager()
{
#if defined(_THREADS) && !defined(NO_FAST_MUTEXES)
    trylock();
    unlock();
#endif
}

/**
 * Start synchronized execution.
 */
void ThreadManager::start_synch() { lock(); }

/**
 * End synchronized execution.
 */
void ThreadManager::end_synch() { unlock(); }

/**
 * Start global synchronized execution.
 */
void ThreadManager::start_global_synch() { global_lock.lock(); }

/**
 * End global synchronized execution.
 */
void ThreadManager::end_global_synch() { global_lock.unlock(); }

ThreadSynchronize::ThreadSynchronize(ThreadManager& sync)
    : s(sync)
{
    s.start_synch();
}

ThreadSynchronize::~ThreadSynchronize() { s.end_synch(); }

SingleThreadObject::SingleThreadObject()
    : ThreadManager()
{
    start_synch();
}

SingleThreadObject::~SingleThreadObject() { end_synch(); }

/*--------------------- class Synchronized -------------------------*/

#ifndef _NO_LOGGING
unsigned int Synchronized::next_id = 0;
#endif

#define ERR_CHK_WITHOUT_EXCEPTIONS(x) \
    do { \
        int result = (x); \
        if (result) { \
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 0); \
            LOG("Constructing Synchronized failed at '" #x \
                "' with (result)"); \
            LOG(result); \
            LOG_END; \
        } \
    } while (0)

Synchronized::Synchronized()
    : cond()
    , monitor()
{
#ifndef _NO_LOGGING
    id = next_id++;
    if (id
        > 1) // static initialization order fiasco: Do not log on first calls
    {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
        LOG("Synchronized created (id)(ptr)");
        LOG(id);
        LOG((unsigned long)this);
        LOG_END;
    }
#endif
    pthread_mutexattr_t attr;
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_init(&attr));
#ifdef AGENTPP_PTHREAD_RECURSIVE
    ERR_CHK_WITHOUT_EXCEPTIONS(
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
#else
    ERR_CHK_WITHOUT_EXCEPTIONS(
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK));
#endif

    memset(&monitor, 0, sizeof(monitor));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutex_init(&monitor, &attr));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_destroy(&attr));

    memset(&cond, 0, sizeof(cond));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_cond_init(&cond, 0));
    isLocked = false;
}

Synchronized::~Synchronized()
{
    int result = pthread_cond_destroy(&cond);
    if (result) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized cond_destroy failed with (result)(ptr)");
        LOG(result);
        LOG((unsigned long)this);
        LOG_END;
    }
    result = pthread_mutex_destroy(&monitor);

#if defined(NO_FAST_MUTEXES)
    if (result == EBUSY) {
        // wait for other threads ...
        if (EBUSY == pthread_mutex_trylock(&monitor)) {
            // another thread owns the mutex,
            if (lock(123)) // NOTE: let's wait, but not forever! CK
            {
                int retries = 0;
                do {
                    pthread_mutex_unlock(&monitor);
                    sleep(13);
                    result = pthread_mutex_destroy(&monitor);
                } while (EBUSY == result
                    && (retries++ < AGENTPP_SYNCHRONIZED_UNLOCK_RETRIES));
            }
        }
    }
#endif

    isLocked = false;
    if (result) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized mutex_destroy failed with (result)(ptr)");
        LOG(result);
        LOG((unsigned long)this);
        LOG_END;
    }
}

void Synchronized::wait() { cond_timed_wait(NULL); }

int Synchronized::cond_timed_wait(const struct timespec* ts)
{
    int result = 0;
    isLocked   = false;
    if (ts) {
        result = pthread_cond_timedwait(&cond, &monitor, ts);
    } else {
        result = pthread_cond_wait(&cond, &monitor);
    }
    isLocked = true;
    return result;
}

bool Synchronized::wait(long timeout)
{
    bool timeoutOccurred = false;
    struct timespec ts   = {};

#if defined(__APPLE__) || defined(HAVE_CLOCK_GETTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    long millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#else
    struct timeval tv = {};
    gettimeofday(&tv, NULL);
    ts.tv_sec   = tv.tv_sec + (time_t)timeout / 1000;
    long millis = tv.tv_usec / 1000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#endif

    isLocked = false;
    int err  = cond_timed_wait(&ts);
    if (err) {
        switch (err) {
        case EINVAL:
            LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
            LOG("Synchronized: wait with timeout returned (error)");
            LOG(err);
            LOG_END;
        // fallthrough
        case ETIMEDOUT:
            timeoutOccurred = true;
            break;
        default:
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Synchronized: wait with timeout returned (error)");
            LOG(err);
            LOG_END;
            break;
        }
    }
    isLocked = true;
    return timeoutOccurred;
}

void Synchronized::notify()
{
    int err = pthread_cond_signal(&cond);
    if (err) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
        LOG("Synchronized: notify failed (err)");
        LOG(err);
        LOG_END;
    }
}

void Synchronized::notify_all()
{
    int err = pthread_cond_broadcast(&cond);
    if (err) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
        LOG("Synchronized: notify_all failed (err)");
        LOG(err);
        LOG_END;
    }
}

bool Synchronized::lock()
{
    int err = pthread_mutex_lock(&monitor);
#ifndef AGENTPP_PTHREAD_RECURSIVE
    if (!err) {
        isLocked = true;
        return true;
    }

    if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive locking detected (id)!");
        LOG(id);
        LOG_END;
        return true;
    }
#else
    if (!err) {
        if (isLocked) {
            // This thread owns already the lock, but
            // we do not like recursive locking. Thus
            // release it immediately and print a warning!
            if (pthread_mutex_unlock(&monitor) != 0) {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 0);
                LOG("Synchronized: unlock failed on recursive lock (id)");
                LOG(id);
                LOG_END;
            } else {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
                LOG("Synchronized: recursive locking detected (id)!");
                LOG(id);
                LOG_END;
            }
        } else {
            isLocked = true;
            // no logging because otherwise deep (virtual endless) recursion
        }
        return true;
    }
#endif

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
    LOG("Synchronized: lock failed (id)(err)");
    LOG(id);
    LOG(err);
    LOG_END;
    return false;
}

bool Synchronized::lock(long timeout)
{
    struct timespec ts = {};

#if defined(__APPLE__) || defined(HAVE_CLOCK_GETTIME)
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    long millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#else
    struct timeval tv = {};
    gettimeofday(&tv, 0);
    ts.tv_sec   = tv.tv_sec + (time_t)timeout / 1000;
    long millis = tv.tv_usec / 1000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec            = (millis % 1000) * 1000000;
#endif

    int error = 0;

#ifdef HAVE_PTHREAD_MUTEX_TIMEDLOCK
    if ((error = pthread_mutex_timedlock(&monitor, &ts)) == 0) {
#else
    long remaining_millis = timeout;
    do {
        error = pthread_mutex_trylock(&monitor);
        if (error == EBUSY) {
            Thread::sleep(10);
            remaining_millis -= 10;
        }
    } while (error == EBUSY && (remaining_millis > 0));
    if (error == 0) {
#endif

#ifndef AGENTPP_PTHREAD_RECURSIVE
        isLocked = true;
        return true; // NOTE: PTHREAD_MUTEX_ERRORCHECK is used! CK
#else
        if (isLocked) {
            // This thread owns already the lock, but
            // we do not like recursive locking. Thus
            // release it immediately and print a warning!
            if (pthread_mutex_unlock(&monitor) != 0) {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 0);
                LOG("Synchronized: unlock failed on recursive lock (id)");
                LOG(id);
                LOG_END;
            } else {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
                LOG("Synchronized: recursive locking detected (id)!");
                LOG(id);
                LOG_END;
            }
        } else {
            isLocked = true;
            // no logging because otherwise deep (virtual endless) recursion
        }
        return true;
#endif

    } else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: lock failed (id)(error)");
        LOG(id);
        LOG(error);
        LOG_END;
        return false;
    }
}

bool Synchronized::unlock()
{
    bool wasLocked = isLocked;
    isLocked       = false;
    int err        = pthread_mutex_unlock(&monitor);
    if (err != 0) {
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
        LOG("Synchronized: unlock failed (id)(error)(wasLocked)");
        LOG(id);
        LOG(err);
        LOG(wasLocked);
        LOG_END;
        isLocked = wasLocked;
        assert(!isLocked);
        return false;
    }
    return true;
}

Synchronized::TryLockResult Synchronized::trylock()
{
#ifndef AGENTPP_PTHREAD_RECURSIVE
    int err = pthread_mutex_trylock(&monitor);
    if (!err) {
        assert(!isLocked);
        isLocked = true;
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: try lock success (id)(ptr)");
        LOG(id);
        LOG((long)this);
        LOG_END;
        return LOCKED;
    }

    if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive try locking detected (id)(ptr)!");
        LOG(id);
        LOG((long)this);
        LOG_END;
        assert(isLocked);
        return OWNED; // NOTE: PTHREAD_MUTEX_ERRORCHECK is used! CK
    }
#else
    if (pthread_mutex_trylock(&monitor) == 0) {
        if (isLocked) {
            // This thread owns already the lock, but
            // we do not like true recursive locking. Thus
            // release it immediately and print a warning!
            if (pthread_mutex_unlock(&monitor) != 0) {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 0);
                LOG("Synchronized: unlock failed on recursive try lock "
                    "(id)(ptr)");
                LOG(id);
                LOG((long)this);
                LOG_END;
            } else {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
                LOG("Synchronized: recursive try locking detected (id)(ptr)!");
                LOG(id);
                LOG((long)this);
                LOG_END;
            }
            return OWNED;
        } else {
            isLocked = true;
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
            LOG("Synchronized: try lock success (id)(ptr)");
            LOG(id);
            LOG((long)this);
            LOG_END;
        }
        return LOCKED;
    }
#endif

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
    LOG("Synchronized: try lock busy (id)(ptr)");
    LOG(id);
    LOG((long)this);
    LOG_END;
    assert(isLocked);
    return BUSY;
}

/*------------------------ class Thread ----------------------------*/

ThreadList Thread::threadList;

void* thread_starter(void* t)
{
    Thread* thread = (Thread*)t;
    Thread::threadList.add(thread);

#ifndef NO_FAST_MUTEXES
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: started (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;
#endif

    try {
        thread->get_runnable().run();
    } catch (...) {
        // OK; ignored CK
    }

#ifndef NO_FAST_MUTEXES
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: ended (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;
#endif

    Thread::threadList.remove(thread);
    // NO! ThreadSanitizer: data race thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread(size_t stack_size)
    : status(IDLE)
    , runnable(this)
    , stackSize(stack_size)
    , tid()
{ }

Thread::Thread(Runnable& r)
    : status(IDLE)
    , runnable(&r)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
    , tid()
{ }

void Thread::run()
{
    LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
    LOG("Thread: empty run method!");
    LOG_END;
}

Thread::~Thread()
{
    if (status == RUNNING) {
        join();
    }
}

Runnable& Thread::get_runnable() { return *runnable; }

void Thread::join()
{
    if (status == RUNNING) {
        void* retstat = NULL;
        int err       = pthread_join(tid, &retstat);
        if (err) {
            status = FINISHED;
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: join failed (error)");
            LOG(err);
            LOG_END;
            return;
        }

        status = IDLE;
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 4);
        LOG("Thread: joined thread successfully (tid)");
        LOG((AGENTPP_OPAQUE_PTHREAD_T)tid);
        LOG_END;
    } else {
        status = IDLE;
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
        LOG("Thread: thread not running (tid)");
        LOG((AGENTPP_OPAQUE_PTHREAD_T)tid);
        LOG_END;
    }
}

void Thread::start()
{
    if (status == IDLE) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, stackSize);
        int err = pthread_create(&tid, &attr, thread_starter, this);
        if (err) {
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: cannot start thread (error)");
            LOG(err);
            LOG_END;
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
    nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000);
}

void Thread::sleep(long millis, long nanos)
{
    nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000 + nanos);
}

void Thread::nsleep(time_t secs, long nanos)
{
    time_t s = secs + nanos / 1000000000;
    long n   = nanos % 1000000000;

#if defined(__APPLE__) || defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    struct timespec interval  = {};
    struct timespec remainder = {};
    interval.tv_sec           = s;
    interval.tv_nsec          = n;
    while (nanosleep(&interval, &remainder) == -1) {
        if (errno == EINTR) {
            LOG_BEGIN(loggerModuleName, EVENT_LOG | 3);
            LOG("Thread: nsleep interrupted");
            LOG_END;
            interval = remainder;
            continue;
        }

        break;
    }
#else
    struct timeval interval = {};
    interval.tv_sec         = s;
    interval.tv_usec        = n / 1000;
    fd_set writefds, readfds, exceptfds;
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    if (select(0, &writefds, &readfds, &exceptfds, &interval) == -1) {
        if (errno == EINTR) {
            LOG_BEGIN(loggerModuleName, EVENT_LOG | 3);
            LOG("Thread: nsleep interrupted");
            LOG_END;
        }
    }
#endif
}

/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager(ThreadPool* tp, size_t stackSize)
    : thread(*this)
{
    threadPool = tp;
    task       = NULL;
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
    LOG("TaskManager: thread stopped");
    LOG_END;
}

void TaskManager::run()
{
    Lock l(*this);

    while (go) {
        if (task) {
            try {
                task->run(); // NOTE: executes the task
            } catch (...) {
                // OK; ignored CK
            }
            delete task;
            task = NULL;
            if (threadPool->is_one_time_execution()) {
                return;
            }
            //==============================
            threadPool->idle_notification();
            //==============================
        }

        while (go && !task) {
            wait(); // NOTE: until notify signal! CK
        }
    }

    if (task) {
        delete task;
        task = NULL;
    }
}

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
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
    LOG("TaskManager: got already a task");
    LOG_END;
    return false;
}

/*--------------------- class ThreadPool --------------------------*/

void ThreadPool::execute(Runnable* t)
{
    Lock l(*this);

    TaskManager* tm = NULL;
    while (!tm) {
        ArrayCursor<TaskManager> cur;
        for (cur.init(&taskList); cur.get(); cur.next()) {
            tm = cur.get();
            if (tm->is_idle()) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
                LOG("TaskManager: task manager found");
                LOG_END;

                if (tm->set_task(t)) {
                    return; // done
                }

                // task could not be assigned
            }
            tm = NULL;
        }

        if (!tm) {
            wait(1000);
        }
    }
}

/// NOTE: should to be called with lock! CK
void ThreadPool::idle_notification() { notify(); }

/// return true if NONE of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_idle()
{
    Lock l(*this);

    ArrayCursor<TaskManager> cur;
    for (cur.init(&taskList); cur.get(); cur.next()) {
        if (!cur.get()->is_idle()) {
            return false;
        }
    }
    return true; // NOTE: all threads are idle
}

bool ThreadPool::is_busy()
{
    Lock l(*this);

    ArrayCursor<TaskManager> cur;
    for (cur.init(&taskList); cur.get(); cur.next()) {
        if (cur.get()->is_busy()) {
            return true;
        }
    }
    return false;
}

void ThreadPool::terminate()
{
    Lock l(*this);

    ArrayCursor<TaskManager> cur;
    for (cur.init(&taskList); cur.get(); cur.next()) {
        cur.get()->stop();
        cur.get()->notify();
    }
    notify();
}

void ThreadPool::join()
{
    Lock l(*this);

    Array<TaskManager> joined;
    ArrayCursor<TaskManager> cur;
    for (cur.init(&taskList); cur.get(); cur.next()) {
        TaskManager* joining = cur.get();
        if (joined.index(joining) < 0) {
            joining->join();
            joined.add(joining);
            lock();
        }
    }
    joined.clear();
}

ThreadPool::ThreadPool(size_t size)
    : stackSize(AGENTPP_DEFAULT_STACKSIZE)
    , oneTimeExecution(false)
{
    for (size_t i = 0; i < size; i++) {
        taskList.add(new TaskManager(this));
    }
}

ThreadPool::ThreadPool(size_t size, size_t stack_size)
    : stackSize(stack_size)
    , oneTimeExecution(false)
{
    for (size_t i = 0; i < size; i++) {
        taskList.add(new TaskManager(this, stackSize));
    }
}

ThreadPool::~ThreadPool()
{
    ThreadPool::terminate();

    join();
}

/*--------------------- class QueuedThreadPool --------------------------*/

QueuedThreadPool::QueuedThreadPool(size_t size)
    : ThreadPool(size)
    , thread(*this)
    , go(true)
{
    thread.start();
}

QueuedThreadPool::QueuedThreadPool(size_t size, size_t stack_size)
    : ThreadPool(size, stack_size)
    , thread(*this)
    , go(true)
{
    thread.start();
}

QueuedThreadPool::~QueuedThreadPool()
{
    // terminate();
    {
        Lock l(thread);
        go = false;
        thread.notify_all();
    }

    thread.join();

    // NOTE: Prevent ThreadSanitizer: data race on vptr (ctor/dtor vs virtual
    // call) threads.cpp:715 Previous write at
    // Agentpp::ThreadPool::~ThreadPool() threads.cpp:860! CK
    ThreadPool::terminate();
}

/// NOTE: asserted to be called with lock! CK
void QueuedThreadPool::assign(Runnable* t)
{
    TaskManager* tm = NULL;
    ArrayCursor<TaskManager> cur;
    for (cur.init(&taskList); cur.get(); cur.next()) {
        tm = cur.get();
        if (tm->is_idle()) {
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
            LOG("TaskManager: task manager found");
            LOG_END;
            if (!tm->set_task(t)) {
                tm = NULL; // still busy!
            } else {
                break;
            }
        }
        tm = NULL;
    }

    if (!tm) {
        queue.add(t);
        thread.notify();
    }
}

void QueuedThreadPool::execute(Runnable* t)
{
    Lock l(thread);

    if (queue.empty()) {
        assign(t);
    } else {
        queue.add(t);
    }

    thread.notify();
}

void QueuedThreadPool::run()
{
    Lock l(thread);

    while (go) {
        Runnable* t = queue.removeFirst();
        if (t) {
            assign(t);
        }
        thread.wait(1000);
    }
}

unsigned int QueuedThreadPool::queue_length()
{
    Lock l(thread);
    return queue.size();
}

/// NOTE: should to be called with lock! CK
void QueuedThreadPool::idle_notification() { thread.notify(); }

/*--------------------- class MibTask --------------------------*/

void MibTask::run() { (task->called_class->*task->method)(task->req); }

#ifdef NO_FAST_MUTEXES

/*--------------------- class LockRequest --------------------------*/

LockRequest::LockRequest(Synchronized* s)
{
    target        = s;
    waitForLock   = true;
    tryLockResult = BUSY;
    lock();
}

LockRequest::~LockRequest() { unlock(); }

LockQueue::LockQueue()
{
    go = true;
    start();
}

LockQueue::~LockQueue()
{
    go = false;
    lock();
    // wake up queue
    notify();
    unlock();

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("LockQueue: end queue");
    LOG_END;

    // join thread here, before pending list is deleted
    if (is_alive()) {
        join();
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("LockQueue: queue stopped");
    LOG_END;

    pendingRelease.clear();
    pendingLock.clear();
}

void LockQueue::run()
{
    lock();
    while ((!pendingLock.empty()) || (!pendingRelease.empty()) || (go)) {
        while (!pendingRelease.empty()) {
            LockRequest* r = pendingRelease.removeFirst();
            r->target->unlock();
            r->lock();
            r->notify();
            r->unlock();
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
            LOG("LockQueue: unlocked (ptr)");
            LOG((long)r->target);
            LOG_END;
        }
        int pl      = pendingLock.size();
        int pending = pl;
        for (int i = 0; i < pl; i++) {
            LockRequest* r = pendingLock.removeFirst();
            // Only if target is not locked at all - also not by
            // this lock queue - then inform requester:
            Synchronized::TryLockResult tryLockResult = r->target->trylock();
            if (tryLockResult == LOCKED) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
                LOG("LockQueue: lock (ptr)(pending)");
                LOG((long)r->target);
                LOG(pending);
                LOG_END;
                r->tryLockResult = LOCKED;
                r->lock();
                r->notify();
                r->unlock();
                pending--;
            } else if (!r->waitForLock) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
                LOG("LockQueue: trylock (ptr)(pending)(result)");
                LOG((long)r->target);
                LOG(pending);
                LOG(tryLockResult == LOCKED     ? "locked"
                        : tryLockResult == BUSY ? "busy"
                                                : "owned");
                LOG_END;
                r->tryLockResult = tryLockResult;
                r->lock();
                r->notify();
                r->unlock();
                pending--;
            } else {
                pendingLock.addLast(r);
            }
        }
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
        LOG("LockQueue: waiting for next event (pending)");
        LOG(pending);
        LOG_END;

        // do not wait forever because we cannot
        // be sure that all instrumentation code notifies
        // us correctly.
        wait(5000);
    }
    unlock();
    LOG_BEGIN(loggerModuleName, INFO_LOG | 2);
    LOG("LockQueue: stopped (pending locks)(pending releases)(go)");
    LOG(pendingLock.size());
    LOG(pendingRelease.size());
    LOG(go);
    LOG_END;
}

void LockQueue::acquire(LockRequest* r)
{
    lock();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
    LOG("LockQueue: adding lock request (ptr)");
    LOG((long)r->target);
    LOG_END;
    pendingLock.addLast(r);
    notify();
    unlock();
}

void LockQueue::release(LockRequest* r)
{
    lock();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
    LOG("LockQueue: adding release request (ptr)");
    LOG((long)r->target);
    LOG_END;
    pendingRelease.addLast(r);
    notify();
    unlock();
}

#endif // NO_FAST_MUTEXES

} // namespace Agentpp
