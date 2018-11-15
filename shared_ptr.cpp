/***
Using shared_ptr to execute code on block exit
==============================================

shared_ptr<void> can automatically execute cleanup code when control leaves a
scope.

Executing f(p), where p is a pointer:
    shared_ptr<void> guard(p, f);

Executing arbitrary code: f(x, y):
    shared_ptr<void> guard(static_cast<void*>(0), bind(f, x, y));

For a more thorough treatment, see the article "Simplify Your Exception-Safe
Code" by Andrei Alexandrescu and Petru Marginean, available online.

Note:
boost::shared_ptr is now part of the C++11 Standard, as std::shared_ptr.

***/

#include <cstdio>
#include <cstdlib>
#include <iostream>

// NOTE: prevent used of C++11 features! ck
// XXX #include <memory>
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
// #include <boost/move/unique_ptr.hpp>


void foo(boost::shared_ptr<int> i) { (*i)++; }


int main()
{
    {
        boost::shared_ptr<int> sp = boost::make_shared<int>(12);
        foo(sp);
        std::cout << *sp << std::endl;
    }

    // C++11 only!
    // auto deleter = [](FILE * ptr)
    // {
    //     std::cout << "[deleter called]\n";
    //     std::fclose(ptr);
    // };

    FILE* fp = std::fopen("/tmp/xxx.dat", "w+");
    if (fp) {
        const boost::shared_ptr<FILE> guard(fp, std::fclose);
        //
        // FIXME prevent use of C++11
        // const boost::movelib::unique_ptr<FILE, decltype(deleter)> guard(fp,
        // deleter);
        //
        // C++11 const std::unique_ptr<FILE, decltype(deleter)> guard(fp,
        // deleter);

        return !std::fwrite("Hallo world!\n", 13, 1, guard.get());
    }

    perror("fopen");
    std::exit(EXIT_FAILURE);
}
