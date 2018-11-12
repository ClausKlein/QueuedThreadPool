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

// #include <errno.h>
// #include <stdlib.h>


namespace Agentpp
{

#ifndef _NO_LOGGING
static const char* loggerModuleName = "agent++.threads";
#endif


/*--------------------- class Synchronized -------------------------*/

#ifndef _NO_LOGGING
unsigned int Synchronized::next_id = 0;
#endif


Synchronized::Synchronized()
    : isLocked(false)
    , flag(false)
{}

Synchronized::~Synchronized()
{
    if (isLocked) {
        flag = false;
        mutex.unlock();
        isLocked = false;
    }
}

void Synchronized::wait() { cond_timed_wait(0); }


int Synchronized::cond_timed_wait(const struct timespec* ts)
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    if (!ts) {
        boost::unique_lock<boost::mutex> l(mutex);
        while (!flag) {
            cond.wait(l);
        }
    } else {
        duration d = sec(ts->tv_sec) + ns(ts->tv_nsec);
        boost::unique_lock<boost::mutex> l(mutex);
        while (!flag) {
            if (cond.wait_for(l, d) == boost::cv_status::timeout) {
                return -1;
            }
        }
    }
    return flag;
}

bool Synchronized::wait(unsigned long timeout)
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    duration d = ms(timeout);
    boost::unique_lock<boost::mutex> l(mutex);
    while (!flag) {
        if (cond.wait_for(l, d) == boost::cv_status::timeout) {
            return -1;
        }
    }
    return flag;
}

void Synchronized::notify()
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    boost::unique_lock<boost::mutex> l(mutex);
    flag = true;
    cond.notify_all();
}

void Synchronized::notify_all()
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    boost::unique_lock<boost::mutex> l(mutex);
    flag = true;
    cond.notify_all();
}

bool Synchronized::lock()
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    mutex.lock();
    return (isLocked = true);
}

bool Synchronized::lock(unsigned long timeout)
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    duration d = ms(timeout);
    boost::unique_lock<boost::mutex> l(mutex);
    while (!flag) {
        if (cond.wait_for(l, d) == boost::cv_status::timeout) {
            return false;
        }
    }
    return true; // OK
}

bool Synchronized::unlock()
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    if (isLocked) {
        isLocked = false;
        mutex.unlock();
        return true;
    }

    return false;
}

Synchronized::TryLockResult Synchronized::trylock()
{
    std::cout << BOOST_CURRENT_FUNCTION << std::endl;

    boost::unique_lock<boost::mutex> l(mutex, boost::defer_lock);
    if (l.try_lock()) {
        isLocked = true;
        l.release();
        return LOCKED;
    }
    return BUSY;
}


/*------------------------ class Thread ----------------------------*/

ThreadList Thread::threadList;

void* thread_starter(void* t)
{
    Thread* thread = static_cast<Thread*>(t);
    Thread::threadList.add(thread);

    // LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    // LOG("Thread: started (tid)");
    // LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    // LOG_END;

#if defined(__APPLE__) && defined(_DARWIN_C_SOURCE)
    // XXX pthread_setname_np(AGENTX_DEFAULT_THREAD_NAME);
#endif

    thread->get_runnable()->run();

    // LOG_BEGIN(loggerModuleName, DEBUG_LOG | 1);
    // LOG("Thread: ended (tid)");
    // LOG((AGENTPP_OPAQUE_PTHREAD_T)(thread->tid));
    // LOG_END;

    Thread::threadList.remove(thread);
    thread->status = Thread::FINISHED;

    return t;
}

Thread::Thread()
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
// XXX , tid(0)
{
    runnable = static_cast<Runnable*>(this);
}

Thread::Thread(Runnable* r)
    : status(IDLE)
    , stackSize(AGENTPP_DEFAULT_STACKSIZE)
// XXX , tid(0)
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

void Thread::join() {}

void Thread::start() {}

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
