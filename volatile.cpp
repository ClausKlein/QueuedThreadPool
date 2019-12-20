/***

====================================================
volatile: The Multithreaded Programmer's Best Friend
====================================================

from
http://www.drdobbs.com/cpp/volatile-the-multithreaded-programmers-b/184403766

Summary

When writing multithreaded programs, you can use volatile to your advantage.
You must stick to the following rules:

 * Define all shared objects as volatile.
 * Don't use volatile directly with primitive types.
 * When defining shared classes, use volatile member functions to express
thread safety.

 ***/

#include <boost/thread.hpp>
#include <iostream>

using namespace boost;


template <typename T> class LockingPtr {
public:
    // Constructors/destructors
    LockingPtr(const volatile T& obj, const volatile mutex& mtx)
        : pObj_(const_cast<T*>(&obj))
        , pMtx_(const_cast<mutex*>(&mtx))
    {
        std::cout << BOOST_CURRENT_FUNCTION << " called" << std::endl;
        pMtx_->lock();
    }
    ~LockingPtr() { pMtx_->unlock(); }
    // Pointer behavior
    T& operator*() { return *pObj_; }
    T* operator->() { return pObj_; }

private:
    T* pObj_;
    mutex* pMtx_;

    // non copyable
    LockingPtr(const LockingPtr&);
    LockingPtr& operator=(const LockingPtr&);
};


/***
Notice the use of overloading.

Now Widget's user can invoke Operation using a uniform syntax either for
volatile objects and get thread safety, or for regular objects and get speed.

The user must be careful about defining the shared Widget objects as volatile.
 ***/
class Widget {
public:
    Widget() {};
    void Operation() const volatile;
    // ...

protected:
    void Operation()
    {
        std::cout << BOOST_CURRENT_FUNCTION << " called" << std::endl;
        Helper();
    };
    void Helper()
    {
        std::cout << BOOST_CURRENT_FUNCTION << " called" << std::endl;
    }

private:
    mutable mutex mtx_;
};


/***
When implementing a volatile member function, the first operation is usually
to lock this with a LockingPtr. Then the work is done by using the non-
volatile sibling:
 ***/
void Widget::Operation() const volatile
{
    LockingPtr<Widget> lpThis(*this, mtx_);
    assert(&(*lpThis) == const_cast<Widget*>(this));

    lpThis->Operation(); // invokes the non-volatile function
}


int main()
{
    volatile Widget wg; // thread save object
    wg.Operation();

    return 0;
}
