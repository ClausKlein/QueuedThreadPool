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

#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#include <boost/assert.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/detail/log.hpp>
#include <boost/thread/future.hpp>

#ifndef BOOST_MSVC
#    include <unistd.h> // _POSIX_THREADS ...
#endif

#include <iostream>

#if !defined(NO_LOGGING) && !defined(NDEBUG)
#    include <boost/current_function.hpp>
#    define LOG_BEGIN(x, y) std::cout << BOOST_CURRENT_FUNCTION << ": "
#    define LOG(x) std::cout << (x) << ' '
#    define LOG_END std::cout << std::endl
#else
#    define LOG_BEGIN(x, y)
#    define LOG(x)
#    define LOG_END
#    define NO_LOGGING 1
#endif

/*
 * Define a macro that can be used for diagnostic output from examples. When
 * compiled -DDEBUG, it results in writing with the specified argument to
 * std::cout. When DEBUG is not defined, it expands to nothing.
 */
#ifdef AGENTPP_DEBUG
#    define DTRACE(arg) \
        BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << ": " << arg \
                         << BOOST_THREAD_END_LOG
#    define THIS_THREAD_YIELD boost::this_thread::sleep_for(ms(10));
#else
#    define DTRACE(arg)
#    ifndef AGENTPP_USE_YIELD
#        define THIS_THREAD_YIELD \
            while (false) \
                ;
#    else
#        define THIS_THREAD_YIELD boost::this_thread::yield();
#    endif
#endif

namespace Agentpp
{

#ifndef NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif

/*--------------------- class Synchronized -------------------------*/

Synchronized::Synchronized()
    : signal(false)
    , tid_(boost::thread::id())
{ }

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

    // NOTE: now we wait for a call to notify or notify_all! CK
    signal = false;
    wait_for_signal_if_needed(l, signal);
    l.release(); // ownership
}

// TODO: should be bool wait_for(duration)
// return false if the predicate pred still evaluates to false after the
// rel_time timeout expired, otherwise true.
bool Synchronized::wait_for(unsigned long timeout)
{
    DTRACE(signal);
    BOOST_ASSERT(is_locked_by_this_thread());

    // NOTE: this may throw! CK
    scoped_lock l(mutex, boost::adopt_lock);

    duration d   = ms(timeout);
    time_point t = Clock::now() + d;

    // NOTE: now we wait for a call to notify or notify_all! CK
    signal = false;
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

    duration d = ms(timeout);
    while (!mutex.try_lock()) {
        boost::this_thread::sleep_for(ms(10));
        d = d - ms(10);
        if (d <= ms(0)) {
            return false;
        }
    }

    BOOST_ASSERT(!is_locked_by_this_thread());
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
    if (is_locked_by_this_thread()) {
        DTRACE("OWNED");
        return OWNED; // true
    }

    scoped_lock l(mutex, boost::try_to_lock);
    if (l.owns_lock()) {
        tid_ = boost::this_thread::get_id();
        l.release(); // ownership
        DTRACE("LOCKED");
        return LOCKED; // true
    }

    return BUSY; // false
}

/*------------------------ class Thread ----------------------------*/

// XXX ThreadList Thread::threadList;

void* Thread::thread_starter(void* t)
{
    Thread* thread = static_cast<Thread*>(t);
    {
        thread->lock();
        // XXX Thread::threadList.add(thread);
        DTRACE("add to threadList");
        thread->unlock();
    }

#ifdef POSIX_THREADS
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 4);
    LOG("Thread: started (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;
#endif

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

#ifdef POSIX_THREADS
    LOG_BEGIN(loggerModuleName, DEBUG_LOG | 4);
    LOG("Thread: ended (tid)");
    LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    LOG_END;
#endif

    {
        thread->lock();
        // XXX Thread::threadList.remove(thread);
        thread->status = Thread::FINISHED;
        DTRACE("removed from threadList");
        thread->unlock();
    }

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
    throw std::runtime_error("Error: empty run method called!");
}

Thread::~Thread()
{
    if (status != IDLE) {
        join();
        DTRACE("Thread joined");
    }
}

Runnable* Thread::get_runnable() const { return runnable; }

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
        DTRACE("thread joined");
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

#    if defined(__INTEGRITY)
        pthread_attr_setthreadname(&attr, AGENTX_DEFAULT_THREAD_NAME);
#    elif defined(__APPLE__)
// NOTE: must be set from within the thread (can't specify thread ID)
// NO! pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#    endif

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
#else
    if (status == IDLE) {
        // NOTE: this may throw! CK
        tid    = boost::thread(boost::bind(thread_starter, this));
        status = RUNNING;
        DTRACE("thread created");
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
    // NOTE: only for TCOV CK
    nsleep((time_t)(millis / 1000), (millis % 1000) * 1000000 + nanos);
    // TODO boost::this_thread::sleep_for(ms(millis) + ns(nanos));
}

void Thread::nsleep(time_t secs, long nanos)
{
    // XXX time_t s = secs + nanos / 1000000000;
    // XXX long n   = nanos % 1000000000;
    boost::this_thread::sleep_for(sec(secs) + ns(nanos));
}

/*--------------------- class TaskManager --------------------------*/

TaskManager::TaskManager( // TODO std::shared_ptr<ThreadPool> tp,
    ThreadPool* tp, size_t stack_size)
    : thread(this)
    , threadPool(0)
{
    DTRACE("");
    threadPool = tp;
    task       = 0;
    go         = true;
    thread.set_stack_size(stack_size);
    thread.start();
    DTRACE("thread started");
}

TaskManager::~TaskManager()
{
    {
        Lock l(*this);
        DTRACE("");
        go = false;
        l.notify();
    }

    thread.join();
    DTRACE("thread joined");
}

void TaskManager::run()
{
    scoped_lock l(mutex);
    //=================================
    tid_ = boost::this_thread::get_id();

    DTRACE("");
    while (go) {
        wait_until_condition(l, boost::bind(&TaskManager::has_task, this));

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
    //=================================
}

bool TaskManager::set_task(Runnable* t)
{
#ifndef AGENTPP_SET_TASK_USE_TRY_LOCK
    // FIXME: may deadlock when called from ThreadPool::execute()! CK
    Lock l(*this);
    if (!task) {
        task = t;
        l.notify();
        DTRACE("after notify");
        return true;
    }
#else
    //=====================<<<
    if (!task && try_lock()) {
        if (task) {
            DTRACE("Busy; too late!");
            (void)unlock();
            //=============>>>
            return false;
        }

        task = t;
        notify();
        DTRACE("after notify");
        (void)unlock();
        //==================>>>
        return true;
    }
#endif

    return false;
}

/*--------------------- class ThreadPool --------------------------*/

void ThreadPool::execute(Runnable* t)
{
    Lock l(*this);
    DTRACE("");

    for (;;) {
        for (std::vector<std::unique_ptr<TaskManager> >::iterator cur =
                 taskList.begin();
             cur != taskList.end(); ++cur) {
            if ((*cur)->is_idle()) {
                DTRACE("task manager found");
                if ((*cur)->set_task(t)) {
                    return; // done
                }
            }
        }
        DTRACE("Busy! Synchronized::wait()");
        wait_for(rand() % 113); // wait_for(ms)
        // TODO wait(); // NOTE: forever until idle_notification() CK
    }
}

void ThreadPool::idle_notification()
{
    Lock l(*this);
    DTRACE("");
    l.notify();
}

/// return true if NONE of the threads in the pool is currently executing any
/// task.
bool ThreadPool::is_idle()
{
    Lock l(*this);
    for (std::vector<std::unique_ptr<TaskManager> >::iterator cur =
             taskList.begin();
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
    for (std::vector<std::unique_ptr<TaskManager> >::iterator cur =
             taskList.begin();
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
    DTRACE("");
    for (std::vector<std::unique_ptr<TaskManager> >::iterator cur =
             taskList.begin();
         cur != taskList.end(); ++cur) {
        (*cur)->stop();
    }
    l.notify_all(); // NOTE: for wait() at execute()
}

ThreadPool::ThreadPool(size_t size)
    : stackSize(AGENTPP_DEFAULT_STACKSIZE)
{
    DTRACE("");

    for (size_t i = 0; i < size; i++) {
        taskList.push_back(std::make_unique<TaskManager>(this));
        // TODO
        // taskList.push_back(std::make_unique<TaskManager>(shared_from_this()));
    }
}

ThreadPool::ThreadPool(size_t size, size_t stack_size)
    : stackSize(stack_size)
{
    DTRACE("");

    for (size_t i = 0; i < size; i++) {
        taskList.push_back(std::make_unique<TaskManager>(this, stackSize));
        // TODO std::make_unique<TaskManager>(shared_from_this(), stackSize));
    }
}

ThreadPool::~ThreadPool()
{
    DTRACE("");

    //    terminate(); // FIXME: warning: Call to virtual function during
    //    destruction
    //
    //    for (size_t i = 0; i < taskList.size(); i++) {
    //        taskList[i].reset();
    //    }
}

/*--------------------- class QueuedThreadPool --------------------------*/

QueuedThreadPool::QueuedThreadPool(size_t size)
    : ThreadPool(0)
    , Thread(this)
    , _size(size)
    , go(true)
{
    DTRACE("");
    if (!_size) {
        this->stop(); // warning: Call to virtual function during construction
    } else {
        ea = std::make_unique<boost::basic_thread_pool>(_size);
    }
}

QueuedThreadPool::QueuedThreadPool(size_t size, size_t stack_size)
    : ThreadPool(0, stack_size)
    , Thread(this)
    , _size(size)
    , go(true)
{
    DTRACE("");
    if (!_size) {
        this->stop(); // warning: Call to virtual function during construction
    } else {
        ea = std::make_unique<boost::basic_thread_pool>(_size);
    }
}

QueuedThreadPool::~QueuedThreadPool()
{
    DTRACE("");
    this->stop();
    ThreadPool::terminate(); // FIXME: Call to virtual function during
                             // destruction
}

#if 0
bool QueuedThreadPool::assign(Runnable* task, bool withQueuing)
{
    DTRACE("");
    return true;
}
#endif

void QueuedThreadPool::execute(Runnable* t)
{
    DTRACE("");
    std::shared_ptr<Runnable> ptr_t(t);
    if (ea && !ea->closed()) {
        boost::future<void> t1 =
            boost::async(*ea, (boost::bind(&Runnable::run, ptr_t)));
    }
}

#if 0
void QueuedThreadPool::run()
{
    DTRACE("");
    while (go) {
        ThreadPool::wait();

        if (!go)
            break;
    }
}
#endif

size_t QueuedThreadPool::queue_length() { return (is_busy() ? 1 : 0); }

void QueuedThreadPool::idle_notification() { }

bool QueuedThreadPool::is_idle()
{
    return !_size || (ea && (!ea->closed() && !ea->try_executing_one()));
}

bool QueuedThreadPool::is_busy()
{
    return !_size || (ea && (ea->closed() || ea->try_executing_one()));
}

void QueuedThreadPool::terminate() { this->stop(); }

void QueuedThreadPool::stop()
{
    DTRACE("");
    go = false;
    if (ea) {
        ea->close();
    }
}

} // namespace Agentpp
