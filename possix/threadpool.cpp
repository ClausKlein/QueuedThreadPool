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
-DAGENTPP_USE_THREAD_POOL agent++/src/threads.cpp > threadpool.cpp
  _##
  _##########################################################################*/

#include "possix/threadpool.hpp"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#    include <assert.h>
#    include <windows.h> // Sleep()
#else
#    include <sys/time.h> // gettimeofday()
#endif

namespace AgentppCK
{

#ifndef NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif

/*--------------------- class Synchronized -------------------------*/

#ifndef NO_LOGGING
int Synchronized::next_id = 0;
#endif

#define ERR_CHK_WITHOUT_EXCEPTIONS(x) \
    do { \
        int err = (x); \
        if (err) { \
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 0); \
            LOG("Constructing Synchronized failed at '" #x "' with (err)"); \
            LOG(strerror(err)); \
            LOG_END; \
        } \
    } while (0)

Synchronized::Synchronized()
{
#ifndef NO_LOGGING
    id = next_id++;
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 9);
    LOG("Synchronized created (id)(ptr)");
    LOG(id);
    LOG((void*)this);
    LOG_END;
#endif

    pthread_mutexattr_t attr;
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_init(&attr));

#ifdef PTHREAD_MUTEX_ERRORCHECK
#    warning "PTHREAD_MUTEX_ERRORCHECK set"
#endif

    ERR_CHK_WITHOUT_EXCEPTIONS(
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK));

    memset(&monitor, 0, sizeof(monitor));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutex_init(&monitor, &attr));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_destroy(&attr));

    memset(&cond, 0, sizeof(cond));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_cond_init(&cond, 0));
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

#ifdef NO_FAST_MUTEXES
#    error "NO_FAST_MUTEXES set"
    //###FIXME### check this! CK
    if (error == EBUSY) {
        // wait for other threads ...
        if (EBUSY == pthread_mutex_trylock(&monitor)) {
            // TODO: another thread owns the mutex, let's wait ... forever? CK
            pthread_mutex_lock(&monitor);
        }
        do {
            (void)pthread_mutex_unlock(&monitor);
            error = pthread_mutex_destroy(&monitor);
            if (error) {
                throw std::runtime_error("pthread_mutex_destroy: failed");
            }
        } while (EBUSY == error); // FIXME: possible endless loop! CK
    }
#endif

    if (error) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized mutex_destroy failed with (error)(ptr)");
        LOG(error);
        LOG((void*)this);
        LOG_END;
    }
}

void Synchronized::wait()
{
#ifndef _WIN32
    cond_timed_wait(NULL);
#else
    // not implemented! wait(INFINITE);
    pthread_cond_wait(&cond, &monitor); // NOTE: FOREVER! CK
#endif
}

#ifndef _WIN32
int Synchronized::cond_timed_wait(const struct timespec* ts)
{
    // NOTE from:
    // http://man7.org/linux/man-pages/man3/pthread_cond_timedwait.3p.html

    // Timed Wait Semantics:
    // For cases when the system clock is advanced discontinuously by an
    // operator, it is expected that implementations process any timed wait
    // expiring at an intervening time as if that time had actually occurred.

    int result;
    if (ts) {
        result = pthread_cond_timedwait(&cond, &monitor, ts);
    } else {
        result = pthread_cond_wait(&cond, &monitor);
    }
    return result;
}
#endif

bool Synchronized::wait(unsigned long timeout)
{
    bool timeoutOccurred = false;

#if defined(_WIN32)
    assert("not implemented function called!");
    return timeoutOccurred;
#else
    struct timespec ts;

#    if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    int millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#    else
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts.tv_sec  = tv.tv_sec + (time_t)timeout / 1000;
    int millis = tv.tv_usec / 1000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#    endif

    int err = cond_timed_wait(&ts);
    if (err) {
        switch (err) {
        case EINVAL:
            LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
            LOG("Synchronized: wait with timeout returned (error)");
            LOG(strerror(err));
            LOG_END;
        // fallthrough
        case ETIMEDOUT:
            timeoutOccurred = true;
            break;
        default:
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Synchronized: wait with timeout returned (error)");
            LOG(strerror(err));
            LOG_END;
            break;
        }
    }
#endif // !defined(_WIN32)

    return timeoutOccurred;
}

void Synchronized::notify()
{
    int err = pthread_cond_signal(&cond);
    if (err) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
        LOG("Synchronized: notify failed (err)");
        LOG(strerror(err));
        LOG_END;
    }
}

void Synchronized::notify_all()
{
    int err = pthread_cond_broadcast(&cond);
    if (err) {
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
        LOG("Synchronized: notify_all failed (err)");
        LOG(strerror(err));
        LOG_END;
    }
}

bool Synchronized::lock()
{
    int err = pthread_mutex_lock(&monitor);
    if (!err) {
        // no logging because otherwise deep (virtual endless) recursion
        return true;
    } else if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive locking detected (id)!");
        LOG(id);
        LOG_END;

        throw std::runtime_error("lock: recursive locking detected");

        return true;
    } else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: lock failed (id)");
        LOG(id);
        LOG_END;

        return false;
    }
}

#if defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS > 0
bool Synchronized::lock(unsigned long timeout)
{
    struct timespec ts;

#    if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    int millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#    else
#        warning "gettimeofday() used"
    struct timeval tv;
    gettimeofday(&tv, 0);
    ts.tv_sec  = tv.tv_sec + (time_t)timeout / 1000;
    int millis = tv.tv_usec / 1000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#    endif

    int error = pthread_mutex_timedlock(&monitor, &ts);
    if (!error) {
        // no logging because otherwise deep (virtual endless) recursion
        return true;
    } else if (error == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking. Thus
        // release it immediately and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive locking detected (id)!");
        LOG(id);
        LOG_END;

        throw std::runtime_error("timedlock: recursive lock detected");

        return true;
    } else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: lock failed (id)(error)");
        LOG(id);
        LOG(error);
        LOG_END;

        return false;
    }
}
#endif

bool Synchronized::unlock()
{
    int err = pthread_mutex_unlock(&monitor);
    if (err) {
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 1);
        LOG("Synchronized: unlock failed (id)(error)");
        LOG(id);
        LOG(strerror(err));
        LOG_END;
        return false;
    }

    return true;
}

Synchronized::TryLockResult Synchronized::trylock()
{
    int err = pthread_mutex_trylock(&monitor);
    if (!err) {
        return LOCKED;
    } else if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive trylocking detected (id)!");
        LOG(id);
        LOG_END;

        throw std::runtime_error("trylock: recursive lock detected");

        return OWNED;
    } else {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
        LOG("Synchronized: try lock busy (id)");
        LOG(id);
        LOG_END;
        return BUSY;
    }
}

/*------------------------ class Thread ----------------------------*/

ThreadList Thread::threadList;

void* thread_starter(void* t)
{
    Thread* thread = (Thread*)t;
    Thread::threadList.add(thread);

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: started (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;

#if defined(__APPLE__)
    pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#elif defined(__linux__) && defined(_GNU_SOURCE)
    pthread_setname_np(pthread_self(), AGENTX_DEFAULT_THREAD_NAME);
#endif

    try {
        thread->get_runnable()->run();
    } catch (std::exception& ex) {
        DTRACE("Exception: " << ex.what());
    } catch (...) {
        // OK; ignored CK
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: ended (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;

    Thread::threadList.remove(thread);
    // TODO: ThreadSanitizer: data race thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread()
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE) // XXX , tid(0)
{
    runnable = (Runnable*)this;
}

Thread::Thread(Runnable* r)
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE) // XXX , tid(0)
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
    if (status != IDLE) {
        join();
        DTRACE("Thread joined");
    }
}

Runnable* Thread::get_runnable() { return runnable; }

void Thread::join()
{
    if (status != IDLE) {
        void* retstat;
        int err = pthread_join(tid, &retstat);
        if (err) {
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: join failed (error)");
            LOG(strerror(err));
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

#if defined(__INTEGRITY)
        pthread_attr_setthreadname(&attr, AGENTX_DEFAULT_THREAD_NAME);
#endif

        pthread_attr_setstacksize(&attr, stackSize);
        int err = pthread_create(&tid, &attr, thread_starter, this);
        if (err) {
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: cannot start thread (error)");
            LOG(strerror(err));
            LOG_END;

            DTRACE("Error: cannot start thread!");
            status = FINISHED; // NOTE: we are not started, see join()! CK
        } else {
            status = RUNNING;

#if defined(__linux__) && defined(_GNU_SOURCE)
            pthread_setname_np(tid, AGENTX_DEFAULT_THREAD_NAME);
#endif
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
#ifdef _WIN32
    Sleep(millis);
#else
    nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000);
#endif
}

void Thread::sleep(long millis, long nanos)
{
#ifdef _WIN32
    sleep(millis);
#else
    nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000 + nanos);
#endif
}

void Thread::nsleep(time_t secs, long nanos)
{
#ifdef _WIN32
    DWORD millis = secs * 1000 + nanos / 1000000;
    Sleep(millis);
#else
    time_t s = secs + nanos / 1000000000;
    long n   = nanos % 1000000000;
#    if defined(__APPLE__) || defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
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
#    else
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
#    endif
#endif
}

/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager(ThreadPool* tp, size_t stackSize)
    : thread(this)
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
    LOG("TaskManager: thread joined");
    LOG_END;
}

void TaskManager::run()
{
    Lock l(*this);
    while (go) {
        if (task) {
            try {
                task->run(); // NOTE: executes the task
            } catch (std::exception& ex) {
                DTRACE("Exception: " << ex.what());
            } catch (...) {
                // OK; ignored CK
            }
            delete task;
            task = NULL;
            //==============================
            // NOTE: may direct call set_task()
            // via QueuedThreadPool::run() => QueuedThreadPool::assign()
            threadPool->idle_notification();
            //==============================
        } else {
            wait(); // NOTE: idle, wait until notify signal CK
        }
    }
    if (task) {
        delete task;
        task = NULL;
        DTRACE("task deleted after stop()");
    }
}

bool TaskManager::is_idle()
{
    Lock l(*this);
    return (!task && thread.is_alive());
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
    Lock l(*this);

    if (taskList.empty()) {
        delete t;
        return;
    }

    TaskManager* tm = NULL;
    while (!tm) {
        for (std::vector<TaskManager*>::iterator cur = taskList.begin();
             cur != taskList.end(); ++cur) {
            tm = *cur;
            if (tm->is_idle()) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
                LOG("TaskManager: task manager found");
                LOG_END;

                if (tm->set_task(t)) {
                    return; // done
                } else {
                    // task could not be assigned
                    tm = NULL;
                }
            }
            tm = NULL;
        }
        if (!tm) {
            DTRACE("busy! Synchronized::wait()");
            wait(1234); // NOTE: for idle_notification ... CK
        }               // ms
    }
}

// NOTE: asserted to be called with lock! CK
void ThreadPool::idle_notification()
{
    // FIXME: needed? CK Lock l(*this);

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("notify");
    LOG_END;

    notify();
}

/// return true if NONE of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_idle()
{
    Lock l(*this);
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        if (!(*cur)->is_idle()) {
            return false;
        }
    }
    return true; // NOTE: all threads are idle
}

/// return true if ALL of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_busy()
{
    Lock l(*this);
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        if ((*cur)->is_idle()) {
            return false;
        }
    }
    return true; // NOTE: all threads are busy
}

void ThreadPool::terminate()
{
    Lock l(*this);
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        (*cur)->stop();
    }
    notify(); // see execute()
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

    // TODO refactor to EmptyTaskList();
    for (size_t i = 0; i < taskList.size(); i++) {
        delete taskList[i]; // implizit Thread::join()
    }
    taskList.clear();
}

/*--------------------- class QueuedThreadPool --------------------------*/

QueuedThreadPool::QueuedThreadPool(size_t size)
    : ThreadPool(size)
    , thread(this)
    , go(true)
{
    thread.start();
}

QueuedThreadPool::QueuedThreadPool(size_t size, size_t stack_size)
    : ThreadPool(size, stack_size)
    , thread(this)
    , go(true)
{
    thread.start();
}

void QueuedThreadPool::EmptyQueue()
{
    Lock l(thread);
    while (!queue.empty()) {
        Runnable* t = queue.front();
        queue.pop();
        if (t) {
            delete t;
            DTRACE("queue entry (task) deleted");
        }
    }
}

QueuedThreadPool::~QueuedThreadPool()
{
    stop();

    thread.join();
    DTRACE("thread joined");

    EmptyQueue();

    ThreadPool::terminate();
}

// NOTE: asserted to be called with lock! CK
bool QueuedThreadPool::assign(Runnable* t, bool /* withQueuing */)
{
    TaskManager* tm = NULL;
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        tm = *cur;
        if (tm->is_idle()) {
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
            LOG("TaskManager: task manager found");
            LOG_END;

            if (tm->set_task(t)) {
                DTRACE("task manager found");
                return true; // OK
            } else {
                tm = NULL;
            }
        }
        tm = NULL;
    }

#if 0
    // NOTE: no idle thread found, push to queue if allowed! CK
    if (!tm && withQueuing) {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
        LOG("queue.push");
        LOG_END;
        queue.push(t);

        DTRACE("busy! task queued; Thread::notify()");
        thread.notify();

        return true;
    }
#endif

    return false;
}

void QueuedThreadPool::execute(Runnable* t)
{
    Lock l(thread);

    if (is_stopped()) {
        delete t;
        return;
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("queue.push");
    LOG_END;
    queue.push(t);
    thread.notify();
}

void QueuedThreadPool::run()
{
    Lock l(thread);

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
        thread.wait(1234); // ms
    }
}

// NOTE: asserted to be called with lock! CK
void QueuedThreadPool::idle_notification()
{
    // FIXME: needed? CK Lock l(thread);

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("notify");
    LOG_END;
    thread.notify();

    // FIXME: needed? CK ThreadPool::idle_notification();
}

bool QueuedThreadPool::is_idle()
{
    Lock l(thread);
    bool result = thread.is_alive() && queue.empty() && ThreadPool::is_idle();

    return result;
}

bool QueuedThreadPool::is_busy()
{
    Lock l(thread);
    bool result =
        !thread.is_alive() || !queue.empty() || ThreadPool::is_busy();

    return result;
}

void QueuedThreadPool::stop()
{
    Lock l(thread);
    go = false;
    thread.notify();
}

} // namespace AgentppCK
