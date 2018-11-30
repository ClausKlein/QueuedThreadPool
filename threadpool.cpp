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

#include "threadpool.hpp"

#include <boost/assert.hpp>
#include <boost/bind.hpp>

#ifndef BOOST_MSVC
#include <unistd.h> // _POSIX_THREADS ...
#endif

#ifdef POSIX_THREADS
#include <pthread.h>
#endif

#include <iostream>

#if !defined(_NO_LOGGING) && !defined(NDEBUG)
#include <boost/current_function.hpp>
#define LOG_BEGIN(x, y) std::cout << BOOST_CURRENT_FUNCTION << ": "
#define LOG(x) std::cout << (x) << ' '
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
#define THIS_THREAD_YIELD boost::this_thread::sleep_for(ms(10));
#else
#define DTRACE(arg)
#ifdef NO_YIELD
#define THIS_THREAD_YIELD \
    while (false) \
        ;
#else
#define THIS_THREAD_YIELD boost::this_thread::yield();
#endif
#endif

namespace Agentpp
{

#ifndef _NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif

/*--------------------- class Synchronized -------------------------*/

Synchronized::Synchronized()
    : signal(false)
    , tid_(boost::thread::id())
{}

Synchronized::~Synchronized()
{
    DTRACE(signal);
    // NOTE: give other waiting threads a time window
    // to return from the wait on our condition_variable
    if (is_locked_by_this_thread()) {
        signal = true;
        notify_all();
        unlock();
        boost::this_thread::sleep_for(ms(10));
    } else {
        while (!signal) {
            if (lock(50)) {
                signal = true; // again! CK
                notify_all();
                unlock();
                boost::this_thread::sleep_for(ms(10));
            }
        }
    }
    DTRACE("");
}

void Synchronized::wait()
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    // NOTE: this may throw! CK
    scoped_lock l(mutex, boost::adopt_lock);
    signal = false;
    while (!signal) {
        //=================================
        tid_ = boost::thread::id();
        cond.wait(l);
        tid_ = boost::this_thread::get_id();
        //=================================
    }

    l.release(); // ownership
}

// TODO: should be bool wait_for(duration)
bool Synchronized::wait(unsigned long timeout)
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    // NOTE: this may throw! CK
    scoped_lock l(mutex, boost::adopt_lock);
    signal       = false;
    duration d   = ms(timeout);
    time_point t = Clock::now() + d;
    while (!signal) {
        //=================================
        tid_ = boost::thread::id();
        if (cond.wait_until(l, t) == boost::cv_status::timeout) {
            tid_ = boost::this_thread::get_id();
            l.release(); // ownership
            return false;
        }
        tid_ = boost::this_thread::get_id();
        //=================================
    }

    l.release(); // ownership
    return true;
}

void Synchronized::notify()
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    // NOTE: this may throw! CK
    scoped_lock l(mutex, boost::adopt_lock);
    signal = true;
    cond.notify_one();
    l.release(); // ownership
}

void Synchronized::notify_all()
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    // NOTE: this may throw! CK
    scoped_lock l(mutex, boost::adopt_lock);
    signal = true;
    cond.notify_all();
    l.release(); // ownership
}

bool Synchronized::lock()
{
    DTRACE("");

    if (is_locked_by_this_thread()) {

#ifndef NDEBUG
        throw std::runtime_error("Synchronized::lock(): recursive used!");
#endif

        // TODO: This thread owns already the lock, but we do not like
        // recursive locking. Thus release it immediately and print a
        // warning! Fank Fock
        return false;
    }

    mutex.lock();
    tid_ = boost::this_thread::get_id();
    return true;
}

// TODO: should be bool try_lock_for(duration)
bool Synchronized::lock(unsigned long timeout)
{
    DTRACE(timeout);
    BOOST_ASSERT(!is_locked_by_this_thread());

    duration d = ms(timeout);
    while (!mutex.try_lock()) {
        boost::this_thread::sleep_for(ms(10));
        d = d - ms(10);
        if (d <= ms(0)) {
            return false;
        }
    }

    tid_ = boost::this_thread::get_id();
    return true; // OK
}

bool Synchronized::unlock()
{
    DTRACE("");

    if (is_locked_by_this_thread()) {
        tid_ = boost::thread::id();
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
        tid_ = boost::this_thread::get_id();
        l.release();   // ownership
        return LOCKED; // true
    }

    return BUSY; // false
}

/*------------------------ class Thread ----------------------------*/

ThreadList Thread::threadList;

void* Thread::thread_starter(void* t)
{
    Thread* thread = static_cast<Thread*>(t);
    Thread::threadList.add(thread);

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: started (tid)");
#ifdef POSIX_THREADS
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
#endif
    LOG_END;

#if defined(__APPLE__) && defined(_DARWIN_C_SOURCE)
    pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#endif

    try {
        //=====================================
        thread->get_runnable()->run();
        //=====================================
    } catch (std::exception& e) {
        DTRACE(e.what());
    } catch (...) {
        // TODO: log ... but ignored! CK
    }

    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("Thread: ended (tid)");
#ifdef POSIX_THREADS
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
#endif
    LOG_END;

    Thread::threadList.remove(thread);
    thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread()
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
#ifdef POSIX_THREADS
    , tid(0)
#endif
{
    runnable = static_cast<Runnable*>(this);
}

Thread::Thread(Runnable* r)
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
#ifdef POSIX_THREADS
    , tid(0)
#endif
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
#ifdef POSIX_THREADS
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
#else
    if (status != IDLE) {
        tid.join();
        status = IDLE;
    }
#endif
}

void Thread::start()
{
#ifdef POSIX_THREADS
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
#else
    if (status == IDLE) {
        // NOTE: this may throw! CK
        tid    = boost::thread(boost::bind(thread_starter, this));
        status = RUNNING;
    }
#endif
}

void Thread::sleep(long millis)
{
    // XXX nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000);
    boost::this_thread::sleep_for(ms(millis));
}

void Thread::sleep(long millis, long nanos)
{
    // nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000 + nanos);
    boost::this_thread::sleep_for(ms(millis) + ns(nanos));
}

void Thread::nsleep(time_t secs, long nanos)
{
    // XXX time_t s = secs + nanos / 1000000000;
    // XXX long n   = nanos % 1000000000;
    boost::this_thread::sleep_for(sec(secs) + ns(nanos));
}

/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager(ThreadPool* tp, size_t stack_size)
    : thread(this)
{
    threadPool = tp;
    task       = 0;
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
        l.notify();
    }

    thread.join();
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    LOG("TaskManager: thread joined");
    LOG_END;
}

void TaskManager::run()
{
    scoped_lock l(mutex);
    tid_ = boost::this_thread::get_id();

    while (go) {
        while (!task && go) {
            //=================================
            tid_ = boost::thread::id();
            cond.wait(l); // NOTE: idle, wait for notify signal CK
            tid_ = boost::this_thread::get_id();
            //=================================
        }

        if (!go)
            break;

        if (task) {
            try {
                //=====================================
                task->run();
                //=====================================
            } catch (std::exception& e) {
                DTRACE(e.what());
            } catch (...) {
                // TODO: log ... but ignored! CK
            }
            delete task;
            task = 0;
            threadPool->idle_notification();
        }
    }

    if (task) {
        delete task;
        task = 0;
        DTRACE("task deleted after stop()");
    }

    tid_ = boost::thread::id();
}

bool TaskManager::set_task(Runnable* t)
{
    DTRACE("");

    // FIXME: may deadlock Lock l(*this);

    if (!task && trylock()) {
        task = t;
        // FIXME: l.notify();
        notify();
        DTRACE("after notify");

        (void)unlock();
        return true;
    } else if (!task) {
        task = t;           // NOTE: without lock! CK
        cond.notify_one();  // TODO: check this! CK
        return true;
    }

    return false;
}

/*--------------------- class ThreadPool --------------------------*/

void ThreadPool::execute(Runnable* t)
{
    DTRACE("");

    Lock l(*this);

    TaskManager* tm = 0;
    for (;;) {
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
            tm = 0;
        }
        if (!tm) {
            DTRACE("busy! Synchronized::wait()");
            wait(); // NOTE: wait for idle_notification CK
        }
    }
}

void ThreadPool::idle_notification()
{
    DTRACE("");

    Lock l(*this);
    l.notify();
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

/// return true if ALL of the threads in the pool is currently executing
/// any task.
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
    DTRACE("");

    Lock l(*this);
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        (*cur)->stop();
    }
    l.notify_all(); // NOTE: for wait() at execute()
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
    DTRACE("");

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
    DTRACE("");
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
bool QueuedThreadPool::assign(Runnable* task, bool withQueuing)
{

    TaskManager* tm = 0;
    if (!go || taskList.empty()) {
        DTRACE("ERROR: can't assign to stopped or empty pool!");
        delete task;
        return true;    // OK, but task is discarded! CK
    }

    DTRACE("");
    for (std::vector<TaskManager*>::iterator cur = taskList.begin();
         cur != taskList.end(); ++cur) {
        tm = *cur;
        if (tm->is_idle()) {
            DTRACE("task manager found");
            if (tm->set_task(task)) {
                DTRACE("task manager found");
                return true; // OK
            }
        }
        tm = 0;
    }

#ifdef AGENTPP_QUEUED_THREAD_POOL_USE_ASSIGN
    // NOTE: no idle thread found, push to queue if allowed! CK
    if (!tm && withQueuing) {
        LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
        LOG("QueuedThreadPool::assign(BUSY): queue.add()");
        LOG_END;
        queue.push(task);

        DTRACE("busy! task queued; Thread::notify()");
        Thread::notify();

        return true;
    }
#endif

    return false;
}

void QueuedThreadPool::execute(Runnable* t)
{
    DTRACE("");

    Lock l(*static_cast<Thread*>(this));

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
        l.notify();
    }
}

void QueuedThreadPool::run()
{
    scoped_lock l(Thread::mutex);
    Thread::tid_ = boost::this_thread::get_id();

#ifndef USE_IMPLIZIT_START
    go = true;
#endif

    while (go) {
        while (go && queue.empty()) {
            DTRACE("empty queue");
            //=================================
            Thread::tid_ = boost::thread::id();
            Thread::cond.wait(l);
            Thread::tid_ = boost::this_thread::get_id();
            //=================================
        }

        if (!go)
            break;

        if (!queue.empty()) {
            DTRACE("queue.front");
            Runnable* t = queue.front();
            if (t) {
                if (assign(t, false)) { // NOTE: without queuing! CK
                    queue.pop();        // OK, now we pop this entry
                } else {
                    DTRACE("busy! Thread::sleep()");
                    // NOTE: sleep some ms to prevent notify() loops while
                    // busy!
                    Thread::sleep(rand() % 113); // ms
                }
            }
        }
    }

    Thread::tid_ = boost::thread::id();
}

size_t QueuedThreadPool::queue_length()
{
    Lock l(*static_cast<Thread*>(this));
    size_t length = queue.size();
    return length;
}

void QueuedThreadPool::idle_notification()
{
    DTRACE("");

    Lock l(*static_cast<Thread*>(this));
    l.notify();
}

bool QueuedThreadPool::is_idle()
{
    Lock l(*static_cast<Thread*>(this));
    bool result = is_alive() && queue.empty() && ThreadPool::is_idle();
    return result;
}

bool QueuedThreadPool::is_busy()
{
    Lock l(*static_cast<Thread*>(this));
    bool result = !queue.empty() || ThreadPool::is_busy();
    return result;
}

void QueuedThreadPool::stop()
{
    DTRACE("");

    Lock l(*static_cast<Thread*>(this));
    go = false;
    l.notify();
}

} // namespace Agentpp
