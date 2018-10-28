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

#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include "threadpool.hpp"

#include <errno.h>
#include <stdlib.h>
#include <string.h>


#include <sys/time.h> // gettimeofday()

namespace Agentpp
{

#ifndef _NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif

static const unsigned AGENTPP_SYNCHRONIZED_UNLOCK_RETRIES(1000);


/*--------------------- class Synchronized -------------------------*/

#ifndef _NO_LOGGING
unsigned int Synchronized::next_id = 0;
#endif

#define ERR_CHK_WITHOUT_EXCEPTIONS(x) \
    do { \
        int err = (x); \
        if (err) { \
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 0); \
            LOG("Constructing Synchronized failed at '" #x "' with (err)"); \
            LOG(err); \
            LOG_END; \
        } \
    } while (0)

Synchronized::Synchronized()
{

#ifndef _NO_LOGGING
    id = next_id++;
    if (id
        > 1) // static initialization order fiasco: Do not log on first calls
    {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
        LOG("Synchronized created (id)(ptr)");
        LOG(id);
        LOG((void*)this);
        LOG_END;
    }
#endif

    pthread_mutexattr_t attr;
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_init(&attr));

#ifdef AGENTPP_PTHREAD_RECURSIVE
#warning "PTHREAD_MUTEX_RECURSIVE used!"
    ERR_CHK_WITHOUT_EXCEPTIONS(
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
#else
#warning "PTHREAD_MUTEX_ERRORCHECK used!"
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
    int error = pthread_cond_destroy(&cond);
    if (error) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized cond_destroy failed with (error)(ptr)");
        LOG(error);
        LOG((void*)this);
        LOG_END;
    }
    error = pthread_mutex_destroy(&monitor);

    if (error == EBUSY) {
        // wait for other threads ...
        if (EBUSY == pthread_mutex_trylock(&monitor)) {
            pthread_mutex_lock(
                &monitor); // another thread owns the mutex, let's wait ...
        }
        unsigned retries = 0;
        do {
            (void)pthread_mutex_unlock(&monitor);
            error = pthread_mutex_destroy(&monitor);

#ifdef DEBUG
            if (error) {
                throw std::runtime_error("pthread_mutex_destroy: failed");
            }
#endif

        } while ((EBUSY == error)
            && (retries++ < AGENTPP_SYNCHRONIZED_UNLOCK_RETRIES));
    }

    if (error) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized mutex_destroy failed with (error)(ptr)");
        LOG(error);
        LOG((void*)this);
        LOG_END;

#ifdef DEBUG
        throw std::runtime_error("pthread_mutex_destroy: failed");
#endif
    }
}

void Synchronized::wait() { cond_timed_wait(0); }

int Synchronized::cond_timed_wait(const struct timespec* ts)
{
    int result;
    isLocked = false;
    if (ts) {
        result = pthread_cond_timedwait(&cond, &monitor, ts);
    } else {
        result = pthread_cond_wait(&cond, &monitor);
    }
    isLocked = true;
    return result;
}

bool Synchronized::wait(unsigned long timeout)
{
    bool timeoutOccurred = false;

    struct timespec ts;

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    int millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts.tv_sec  = tv.tv_sec + (time_t)timeout / 1000;
    int millis = tv.tv_usec / 1000 + (timeout % 1000);
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
            LOG("Synchronized: wait with timeout returned (EINVAL)");
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

#ifdef DEBUG
        throw std::runtime_error("notify: failed");
#endif
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

#ifdef DEBUG
        throw std::runtime_error("notify_all: failed");
#endif
    }
}

bool Synchronized::lock()
{
    int err = pthread_mutex_lock(&monitor);

#ifndef AGENTPP_PTHREAD_RECURSIVE
    if (!err) {
        isLocked = true;
        return true;
    } else if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive lock detected (id)!");
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

#ifdef DEBUG
                throw std::runtime_error("lock: unlock failed");
#endif

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

    else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: lock failed (id)(err)");
        LOG(id);
        LOG(err);
        LOG_END;

#ifdef DEBUG
        throw std::runtime_error("lock: failed");
#endif

        return false;
    }
}

bool Synchronized::lock(unsigned long timeout)
{
    struct timespec ts;

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    int millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts.tv_sec  = tv.tv_sec + (time_t)timeout / 1000;
    int millis = tv.tv_usec / 1000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec            = (millis % 1000) * 1000000;
#endif

#ifdef HAVE_PTHREAD_MUTEX_TIMEDLOCK
    int error = pthread_mutex_timedlock(&monitor, &ts);
    if (!error)
#else
    long remaining_millis = timeout;
    int error;
    do {
        error = pthread_mutex_trylock(&monitor);
        if (error == EBUSY) {
            Thread::sleep(10);
            remaining_millis -= 10;
        }
    } while (error == EBUSY && (remaining_millis > 0));
    if (error == 0)
#endif

    {

#ifndef AGENTPP_PTHREAD_RECURSIVE
        isLocked = true;
        return true;
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

#ifdef DEBUG
                throw std::runtime_error("timedlock: unlock failed");
#endif

            } else {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
                LOG("Synchronized: recursive lock detected (id)!");
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

#ifdef DEBUG
        throw std::runtime_error("timedlock: lock failed");
#endif

        return false;
    }
}

bool Synchronized::unlock()
{
    bool wasLocked = isLocked;
    isLocked       = false;
    int err        = pthread_mutex_unlock(&monitor);
    if (err) {
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
        LOG("Synchronized: unlock failed (id)(error)(wasLocked)");
        LOG(id);
        LOG(err);
        LOG(wasLocked);
        LOG_END;
        isLocked = wasLocked;

#ifdef DEBUG
// TBD: throw std::runtime_error("ulock: unlock failed");
#endif

        return false;
    }

    return true;
}

Synchronized::TryLockResult Synchronized::trylock()
{

#ifndef AGENTPP_PTHREAD_RECURSIVE
    int err = pthread_mutex_trylock(&monitor);
    if (!err) {
        isLocked = true;
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: trylock success (id)(ptr)");
        LOG(id);
        LOG((long)this);
        LOG_END;
        return LOCKED;
    } else if ((isLocked) && (err == EBUSY)) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive trylock detected (id)(ptr)!");
        LOG(id);
        LOG((long)this);
        LOG_END;
        return OWNED;
    }
#else
    if (pthread_mutex_trylock(&monitor) == 0) {
        if (isLocked) {
            // This thread owns already the lock, but
            // we do not like true recursive locking. Thus
            // release it immediately and print a warning!
            if (pthread_mutex_unlock(&monitor) != 0) {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 0);
                LOG("Synchronized: unlock failed on recursive trylock "
                    "(id)(ptr)");
                LOG(id);
                LOG((long)this);
                LOG_END;
            } else {
                LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
                LOG("Synchronized: recursive trylock detected (id)(ptr)!");
                LOG(id);
                LOG((long)this);
                LOG_END;
            }
            return OWNED;
        } else {
            isLocked = true;
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
            LOG("Synchronized: trylock success (id)(ptr)");
            LOG(id);
            LOG((long)this);
            LOG_END;
        }
        return LOCKED;
    }
#endif

    else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
        LOG("Synchronized: trylock busy (id)(ptr)");
        LOG(id);
        LOG((long)this);
        LOG_END;
        return BUSY;
    }
}


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

#ifdef _POSIX_TIMERS
    struct timespec interval, remainder;
    interval.tv_sec  = s;
    interval.tv_nsec = n;
    if (nanosleep(&interval, &remainder) == -1) {
        if (errno == EINTR) {
            LOG_BEGIN(loggerModuleName, EVENT_LOG | 3);
            LOG("Thread: sleep interrupted");
            LOG_END;
        }
    }
#else
    struct timeval interval;
    interval.tv_sec  = s;
    interval.tv_usec = n / 1000;
    fd_set writefds, readfds, exceptfds;
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    if (select(0, &writefds, &readfds, &exceptfds, &interval) == -1) {
        if (errno == EINTR) {
            LOG_BEGIN(loggerModuleName, EVENT_LOG | 3);
            LOG("Thread: sleep interrupted");
            LOG_END;
        }
    }
#endif
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
    lock();
    go = false;
    notify();
    unlock();

    thread.join();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("TaskManager: thread joined");
    LOG_END;
}

void TaskManager::run()
{
    lock();
    while (go) {
        if (task) {
            task->run(); // NOTE: executes the task
            delete task;
            task = 0;

            unlock(); // NOTE: prevent deadlock! CK
            //==============================
            // NOTE: may direct call set_task()
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

bool TaskManager::set_task(Runnable* t)
{
    lock();
    if (!task) {
        task = t;
        notify();
        unlock();
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
        LOG("TaskManager: after notify");
        LOG_END;
        return true;
    } else {
        unlock();
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 2);
        LOG("TaskManager: got already a task");
        LOG_END;
        return false;
    }
}


/*--------------------- class ThreadPool --------------------------*/

void ThreadPool::execute(Runnable* t)
{
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
                if (tm->set_task(t)) {
                    return; // done
                } else {
                    // task could not be assigned
                    tm = 0;
                    lock();
                }
            }
            tm = 0;
        }
        if (!tm) {
            DTRACE("busy! Synchronized::wait()");
            wait(1234); // NOTE: (ms) for idle_notification ... CK
        }
    }
}

void ThreadPool::idle_notification()
{
    // FIXME: needed? CK Lock l(*this);
    notify();
}

/// return true if NONE of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_idle()
{
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
{
#ifdef USE_IMPLIZIT_START
    go = true;

    start();
#endif
}

QueuedThreadPool::QueuedThreadPool(size_t size, size_t stack_size)
    : ThreadPool(size, stack_size)
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
            if (!tm->set_task(t)) {
                tm = 0;
                Thread::lock();
            } else {
                Thread::lock();
                DTRACE("task manager found");
                return true; // OK
            }
        }
        tm = 0;
    }

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
                    // NOTE: wait some ms to prevent notify() loops while busy!
                    // CK
                    Thread::sleep(rand() % 113); // ms
                }
            }
        }

        // NOTE: for idle_notification ... CK
        Thread::wait(1234); // ms
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

    // FIXME: why? CK
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
