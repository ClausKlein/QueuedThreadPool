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

#include <cerrno>
#include <cstring> // memset()

#ifdef _WIN32
#    include <windows.h> // Sleep()
#else
#    include <sys/time.h> // gettimeofday()
#endif

#include <stdexcept> // std::runtime_error()

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
    : cond()
    , monitor()
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

    //#ifdef PTHREAD_MUTEX_ERRORCHECK
    //#    warning "PTHREAD_MUTEX_ERRORCHECK set"
    //#endif

    ERR_CHK_WITHOUT_EXCEPTIONS(
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK));

    memset(&monitor, 0, sizeof(monitor));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutex_init(&monitor, &attr));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_mutexattr_destroy(&attr));

    memset(&cond, 0, sizeof(cond));
    ERR_CHK_WITHOUT_EXCEPTIONS(pthread_cond_init(&cond, NULL));
}

Synchronized::~Synchronized()
{
    int error  = 0;
    int errors = 0;

#ifdef NO_FAST_MUTEXES
    do {
        // first try to get the lock
        error = pthread_mutex_trylock(&monitor);
        if (!error) {
            notify_all();
            (void)pthread_mutex_unlock(&monitor);
            // if another thread waits for signal with mutex, let's wait.

#    if defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS > 0
            if (lock(10)) // NOTE: but not forever! CK
#    else
            error = pthread_mutex_lock(&monitor); // NOTE: FOREVER! CK
            if (!error)
#    endif
            {
                (void)pthread_mutex_unlock(&monitor);
                error = pthread_mutex_destroy(&monitor);
                if (error) {
                    ++errors;
                    Thread::sleep(errors * 2);
                }
            }
        } else {
            ++errors;
            Thread::sleep(errors * 2);
        }
        if (errors > 11) {
            break;
        }
    } while (EBUSY == error);
#else
    error              = pthread_mutex_destroy(&monitor);
#endif

    if (error) {
        ++errors;
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized mutex_destroy failed with (error)(ptr)");
        LOG(strerror(error));
        LOG((void*)this);
        LOG_END;
#ifdef NO_FAST_MUTEXES
        // NOTE: this aborts ...
        throw std::runtime_error("pthread_mutex_destroy: failed");
#endif
    }

    error = pthread_cond_destroy(&cond);
    if (error) {
        ++errors;
        LOG_BEGIN(loggerModuleName, ERROR_LOG | 2);
        LOG("Synchronized cond_destroy failed with (error)(ptr)");
        LOG(strerror(error));
        LOG((void*)this);
        LOG_END;
#ifdef NO_FAST_MUTEXES
        // NOTE: this aborts ...
        throw std::runtime_error("pthread_cond_destroy: failed");
#endif
    }
}

void Synchronized::wait()
{
#ifdef _WIN32
    // NOTE: not implemented! wait(INFINITE);
#endif

    int err = pthread_cond_wait(&cond, &monitor); // NOTE: FOREVER! CK
    if (err == EINVAL) {
        throw std::runtime_error(
            "pthread_cond_wait: The cond or the mutex is invalid!");
    }
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
        result = pthread_cond_wait(&cond, &monitor); // NOTE: FOREVER! CK
    }
    return result;
}
#endif

bool Synchronized::wait(unsigned long timeout)
{
    bool timeoutOccurred = false;

#if defined(_WIN32)
    throw std::runtime_error("not implemented function called!");
    return timeoutOccurred;
#else
    struct timespec ts = {};

#    if defined(__APPLE__) || defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)timeout / 1000;
    int millis = ts.tv_nsec / 1000000 + (timeout % 1000);
    if (millis >= 1000) {
        ts.tv_sec += 1;
    }
    ts.tv_nsec = (millis % 1000) * 1000000;
#    else
#        warning "gettimeofday() used"
    struct timeval tv = {};
    gettimeofday(&tv, NULL);
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
    }

    if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive locking detected (id)!");
        LOG(id);
        LOG_END;

        throw std::runtime_error("lock: recursive locking detected");

        return true;
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
    LOG("Synchronized: lock failed (id)");
    LOG(id);
    LOG_END;

    return false;
}

#if defined(_POSIX_TIMEOUTS) && _POSIX_TIMEOUTS > 0
bool Synchronized::lock(unsigned long timeout)
{
    struct timespec ts;

#    if defined(__APPLE__) || defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
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
    ts.tv_sec = tv.tv_sec + (time_t)timeout / 1000;
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
    }

    if (err == EDEADLK) {
        // This thread owns already the lock, but
        // we do not like recursive locking and print a warning!
        LOG_BEGIN(loggerModuleName, WARNING_LOG | 5);
        LOG("Synchronized: recursive trylocking detected (id)!");
        LOG(id);
        LOG_END;

        throw std::runtime_error("trylock: recursive lock detected");

        return OWNED;
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 8);
    LOG("Synchronized: try lock busy (id)");
    LOG(id);
    LOG_END;
    return BUSY;
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
    // NO! ThreadSanitizer: data race thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread(size_t stack_size)
    : status(IDLE)
    , runnable(*this)
    , stackSize(stack_size)
    , tid()
{ }

Thread::Thread(Runnable& r)
    : status(IDLE)
    , runnable(r)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
    , tid()
{ }

void Thread::run()
{
    LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
    LOG("Thread: empty run method!");
    LOG_END;
}

Thread::~Thread() { join(); }

Runnable* Thread::get_runnable() { return &runnable; }

void Thread::join()
{
    if (status == RUNNING) {
        void* retstat;
        int err = pthread_join(tid, &retstat);
        if (err) {
            status = FINISHED;
            LOG_BEGIN(loggerModuleName, ERROR_LOG | 1);
            LOG("Thread: join failed (error)");
            LOG(strerror(err));
            LOG_END;
        } else {
            status = IDLE;
            LOG_BEGIN(loggerModuleName, DEBUG_LOG | 4);
            LOG("Thread: joined thread successfully (tid)");
            LOG((AGENTPP_OPAQUE_PTHREAD_T)tid);
            LOG_END;
        }
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
        int policy               = 0;
        struct sched_param param = {};
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
    time_t s                  = secs + nanos / 1000000000;
    long n                    = nanos % 1000000000;
#    if defined(__APPLE__) || defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
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
            LOG("Thread: nsleep interrupted");
            LOG_END;
        }
    }
#    endif
#endif
}

/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager(ThreadPool* tp, size_t stack_size)
    : thread(*this)
{
    threadPool = tp;
    task       = NULL;
    go         = true;
    thread.set_stack_size(stack_size);
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
        if (taskList.empty()) {
            delete t;
            return;
        }

        for (std::vector<TaskManager*>::iterator cur = taskList.begin();
             cur != taskList.end(); ++cur) {
            tm = *cur;
            if (tm->is_idle()) {
                LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
                LOG("TaskManager: task manager found");
                LOG_END;

                if (tm->set_task(t)) {
                    return; // done
                }
            }
            tm = NULL;
        }
        if (!tm) {
            DTRACE("busy! Synchronized::wait()");
            wait(); // NOTE: for idle_notification ... CK
        }
    }
}

/// NOTE: asserted to be called with lock! CK
void ThreadPool::idle_notification()
{
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

    if (taskList.empty()) {
        return false;
    }

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
#if 0
    // NOTE: this not the same! CK return !ThreadPool::is_idle();
#else
    Lock l(*this);

    if (taskList.empty()) {
        return true;
    }

    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        if ((*cur)->is_idle()) {
            return false;
        }
    }
    return true; // NOTE: all threads are busy
#endif
}

void ThreadPool::EmptyTaskList()
{
    for (size_t i = 0; i < taskList.size(); i++) {
        delete taskList[i]; // implizit Thread::join()
    }
    taskList.clear();
}

void ThreadPool::terminate()
{
    Lock l(*this);

    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        (*cur)->stop();
    }
    EmptyTaskList();

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
    ThreadPool::terminate();

    EmptyTaskList();
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
    terminate();

    thread.join();
    DTRACE("thread joined");

    EmptyQueue();

    ThreadPool::terminate();
}

/// NOTE: asserted to be called with lock! CK
bool QueuedThreadPool::assign(Runnable* t)
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
            }
        }
        tm = NULL;
    }

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
                if (assign(t)) {
                    queue.pop(); // OK, now we pop this entry
                }
            }
        }
        DTRACE("idle! Synchronized::wait()");
        thread.wait(); // NOTE: for idle_notification ... CK
    }
}

/// NOTE: asserted to be called with lock! CK
void QueuedThreadPool::idle_notification()
{
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("notify");
    LOG_END;
    thread.notify();
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
        !thread.is_alive() || !(queue.empty() && ThreadPool::is_idle());

    return result;
}

void QueuedThreadPool::terminate()
{
    Lock l(thread);
    go = false;
    thread.notify();

    ThreadPool::terminate();
}

} // namespace AgentppCK
