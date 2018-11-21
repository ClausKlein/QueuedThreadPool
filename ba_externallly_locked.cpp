// Copyright (C) 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// see too:
// https://www.boost.org/doc/libs/1_68_0/doc/html/thread/synchronization.html#thread.synchronization.tutorial.internal_locking.internal_and_external_locking
//

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID

#include <boost/thread/mutex.hpp>
// TODO: needs that boost/thread/mutex.hpp is included before? CK
#include <boost/thread/externally_locked.hpp>

#include <boost/thread/detail/log.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/executor_adaptor.hpp>
#include <boost/thread/executors/generic_executor_ref.hpp>
#include <boost/thread/executors/thread_executor.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/lockable_adapter.hpp>
#include <boost/thread/strict_lock.hpp>

#include <iostream>

#ifdef BOOST_MSVC
#pragma warning( \
    disable : 4355) // 'this' : used in base member initializer list
#endif


//=========================================================
// The user can always define his default executor himself.
// see too:
// https://www.boost.org/doc/libs/1_68_0/doc/html/thread/synchronization.html#thread.synchronization.executors.ref.basic_thread_pool
boost::generic_executor_ref default_executor()
{
    static boost::basic_thread_pool tp(4);
    return boost::generic_executor_ref(tp);
}
//=========================================================


using namespace boost;

class BankAccount {
    int balance_;

public:
    BankAccount()
        : balance_(0)
    {}

    void Deposit(int amount) { balance_ += amount; }
    void Withdraw(int amount) { balance_ -= amount; }
    int GetBalance() const { return balance_; }
};


class AccountManager : public basic_lockable_adapter<mutex> {
public:
    typedef basic_lockable_adapter<mutex> lockable_base_type;
    AccountManager()
        : lockable_base_type()
        , checkingAcct_(*this)
        , savingsAcct_(*this)
    {}
    inline void Checking2Savings(int amount);
    inline void AMoreComplicatedChecking2Savings(int amount);

private:
    /*<-*/
    bool some_condition()
    {
        static unsigned n(0);
        return ((++n % 2) ? true : false);
    }
    /*->*/
    externally_locked<BankAccount, AccountManager> checkingAcct_;
    externally_locked<BankAccount, AccountManager> savingsAcct_;
};


void AccountManager::Checking2Savings(int amount)
{
    strict_lock<AccountManager> guard(*this);
    checkingAcct_.get(guard).Withdraw(amount);
    std::cout << "checking balance: " << checkingAcct_.get(guard).GetBalance()
              << std::endl;

    savingsAcct_.get(guard).Deposit(amount);
    std::cout << " savings balance: " << savingsAcct_.get(guard).GetBalance()
              << std::endl;
}


void AccountManager::AMoreComplicatedChecking2Savings(int amount)
{
    unique_lock<AccountManager> guard1(*this, defer_lock);
    if (some_condition()) {
        guard1.lock();
    }
    {
        nested_strict_lock<unique_lock<AccountManager> > guard(guard1);

        checkingAcct_.get(guard).Withdraw(amount);
        std::cout << "checking balance: "
                  << checkingAcct_.get(guard).GetBalance() << std::endl;

        savingsAcct_.get(guard).Deposit(amount);
        std::cout << " savings balance: "
                  << savingsAcct_.get(guard).GetBalance() << std::endl;
    }
    // NO! guard1.unlock(); // NOTE: the guard may not locked! CK
    //  And in case of an exception, it would not be unlocked! CK
}


struct simpleTransaktion {
    AccountManager& mgr_;
    int amount_;
    simpleTransaktion(AccountManager& mgr, int amount)
        : mgr_(mgr)
        , amount_(amount)
    {}
    void operator()()
    {
        try {
            BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << "(" << amount_ << ")"
                             << BOOST_THREAD_END_LOG;
            mgr_.Checking2Savings(amount_);
        } catch (std::exception& ex) {
            BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << "(" << ex.what()
                             << ")" << BOOST_THREAD_END_LOG;
        }
    }
};


struct complicatedTransaktion {
    AccountManager& mgr_;
    int amount_;
    complicatedTransaktion(AccountManager& mgr, int amount)
        : mgr_(mgr)
        , amount_(amount)
    {}
    void operator()()
    {
        try {
            BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << "(" << amount_ << ")"
                             << BOOST_THREAD_END_LOG;
            mgr_.AMoreComplicatedChecking2Savings(amount_);
        } catch (std::exception& ex) {
            BOOST_THREAD_LOG << BOOST_CURRENT_FUNCTION << "(" << ex.what()
                             << ")" << BOOST_THREAD_END_LOG;
        }
    }
};


int main()
{
    AccountManager mgr;

    //===========================================================
    // A thread_executor with a threads for each task.
    boost::executor_adaptor<boost::thread_executor> myThreadPool;
    //===========================================================
    {
        unique_lock<AccountManager> guard(mgr); // block the manager!

        simpleTransaktion simple(mgr, 100);
        //================================
        default_executor().submit(simple);
        myThreadPool.submit(simple);
        default_executor().submit(simple);
        //================================

        complicatedTransaktion complicated(mgr, 1000);
        //=====================================
        default_executor().submit(complicated);
        myThreadPool.submit(complicated);
        default_executor().submit(complicated);
        //=====================================
    }

    std::cout << BOOST_CURRENT_FUNCTION << std::endl;
    mgr.AMoreComplicatedChecking2Savings(-123);

    // NOTE: prevent the use of the reference to the mgr! CK
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    myThreadPool.close();   // NOTE: not realy needed! CK

    return 0;
}
