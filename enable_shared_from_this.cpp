//
// from http://en.cppreference.com/w/cpp/memory/enable_shared_from_this
//
#include <iostream>

// NOTE: prevent used of C++11 features! ck
// XXX #include <memory>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

using namespace boost;

struct Good : enable_shared_from_this<Good> // note: public inheritance
{
    shared_ptr<Good> getptr() { return shared_from_this(); }
};

struct Bad {
    shared_ptr<Bad> getptr() { return shared_ptr<Bad>(this); }
    ~Bad() { std::cout << "Bad::~Bad() called\n"; }
};

int main()
{
    // Good: the two shared_ptr's share the same object
    shared_ptr<Good> gp1 = make_shared<Good>();
    shared_ptr<Good> gp2 = gp1->getptr();
    std::cout << "gp2.use_count() = " << gp2.use_count() << '\n';

    // Bad: shared_from_this is called without having shared_ptr owning
    // the caller
    try {
        Good not_so_good;
        shared_ptr<Good> gp1 = not_so_good.getptr();
        std::cout << "gp1.use_count() = " << gp1.use_count() << '\n';
    } catch (bad_weak_ptr& e) {
        // undefined behavior (until C++17)
        // and bad_weak_ptr thrown (since C++17)
        std::cout << "Exception: " << e.what() << '\n';
    }

    // Bad, each shared_ptr thinks it's the only owner of the object
    shared_ptr<Bad> bp1 = make_shared<Bad>();
    shared_ptr<Bad> bp2 = bp1->getptr();
    std::cout << "bp2.use_count() = " << bp2.use_count() << '\n';

    return 0;
} // UB: double-delete of Bad
