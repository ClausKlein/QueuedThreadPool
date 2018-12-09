#!/bin/sh

set +x

make threads_test

./threads_test --list_content
# 0: ./threads_test
# 1: --list_content
# QueuedThreadPoolIndependency_test*
# QueuedThreadPoolInterface_test*
# QueuedThreadPoolLoad_test*
# QueuedThreadPool_busy_test*
# QueuedThreadPool_test*
# SyncDeadlock_test*
# SyncDelete_while_used_test*
# SyncTry_lock_for_test*
# SyncTrylock_test*
# SyncWait_test*
# Synchronized_test*
# ThreadLivetime_test*
# ThreadPool_busy_test*
# ThreadPool_test*
# ThreadSleep_test*
# ThreadTaskThrow_test*
# test_lock_ten_other_thread_locks_in_different_order*
# test_lock_two_other_thread_locks_in_opposite_order*
# test_lock_two_other_thread_locks_in_order*

./threads_test --log_level=message --run_test='Sync*' > Synchronized_test.log 2>&1

./threads_test --log_level=message --run_test=QueuedThreadPool_test > QueuedThreadPool_test.log 2>&1

./threads_test --log_level=message --run_test=ThreadPool_test > ThreadPool_test.log 2>&1

tail *.log
# ==> QueuedThreadPool_test.log <==
# bool Agentpp::Synchronized::unlock(): 
# bool Agentpp::Synchronized::unlock(): 
# void *Agentpp::thread_starter(void *): Thread: ended (tid) 0x700000011000 
# bool Agentpp::Synchronized::lock(): 
# bool Agentpp::Synchronized::unlock(): 
# void Agentpp::Thread::join(): Thread: joined thread successfully (tid) 0x700000011000 
# virtual Agentpp::TaskManager::~TaskManager(): TaskManager: thread joined 
# 
# *** No errors detected
# 
# ==> Synchronized_test.log <==
# bool Agentpp::Synchronized::lock(): 
# Synchronized::TryLockResult Agentpp::Synchronized::trylock(): 
# bool Agentpp::Synchronized::unlock(): 
# bool Agentpp::Synchronized::unlock(): 
# bool Agentpp::Synchronized::lock(): 
# bool Agentpp::Synchronized::wait(unsigned long): 0
# bool Agentpp::Synchronized::unlock(): 
# 
# *** No errors detected
# 
# ==> ThreadPool_test.log <==
# bool Agentpp::Synchronized::unlock(): 
# bool Agentpp::Synchronized::unlock(): 
# void *Agentpp::thread_starter(void *): Thread: ended (tid) 0x70000004a000 
# bool Agentpp::Synchronized::lock(): 
# bool Agentpp::Synchronized::unlock(): 
# void Agentpp::Thread::join(): Thread: joined thread successfully (tid) 0x70000004a000 
# virtual Agentpp::TaskManager::~TaskManager(): TaskManager: thread joined 
# 
# *** No errors detected
# Claus-MBP:Threadpool clausklein$ 
