// Copyright (C) 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4

#include <boost/thread/mutex.hpp>

#include <boost/thread/externally_locked.hpp> //TODO: needs that boost/thread/mutex.hpp is included before? CK
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/lockable_adapter.hpp>
#include <boost/thread/strict_lock.hpp>


#ifdef BOOST_MSVC
#pragma warning( \
    disable : 4355) // 'this' : used in base member initializer list
#endif

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

//[AccountManager
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
    bool some_condition() { return true; }
    /*->*/
    externally_locked<BankAccount, AccountManager> checkingAcct_;
    externally_locked<BankAccount, AccountManager> savingsAcct_;
};
//]

//[Checking2Savings
void AccountManager::Checking2Savings(int amount)
{
    strict_lock<AccountManager> guard(*this);
    checkingAcct_.get(guard).Withdraw(amount);
    savingsAcct_.get(guard).Deposit(amount);
}
//]

//[AMoreComplicatedChecking2Savings
void AccountManager::AMoreComplicatedChecking2Savings(int amount)
{
    unique_lock<AccountManager> guard1(*this);
    // XXX boost::mutex mtx_; // explicit mutex declaration

    if (some_condition()) {
        // ERROR: boost unique_lock owns already the mutex: Resource deadlock
        // avoided  ERROR: guard1.lock();
    }
    {
        // TODO: XXX boost::lock_guard<boost::mutex> sopedGuard(mtx_);
        // FIXME: if(checkingAcct_.get(guard).GetBalcance() >= amount)
        {
            nested_strict_lock<unique_lock<AccountManager> > guard(guard1);
            checkingAcct_.get(guard).Withdraw(amount);
            savingsAcct_.get(guard).Deposit(amount);
        }
        guard1.unlock(); // NOTE: the guard1 may not locked! CK
    }
}
//]


int main()
{
    AccountManager mgr;

    mgr.Checking2Savings(100);
    mgr.AMoreComplicatedChecking2Savings(1000);

    return 0;
}
