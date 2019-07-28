#undef _POSIX_C_SOURCE
#include <boost/assert.hpp>
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp> // tss(Class thread specific storage) -> tls: thread local storage

#include <iostream>

boost::mutex
    mutex; // warning: initialization of 'mutex' with static storage duration
           // may throw an exception that cannot be caught [cert-err58-cpp]


void init()
{
    static boost::thread_specific_ptr<bool> tls;
    if (!tls.get()) {
        tls.reset(new bool(true));
        boost::lock_guard<boost::mutex> lock(mutex);
        std::cout << "done" << '\n';
    } else if (*tls) {
        assert(*tls == true);
        *tls = false;
        boost::lock_guard<boost::mutex> lock(mutex);
        std::cout << "set to false" << '\n';
    }
}

void task()
{
    init();
    init(); // again ...
}

int main()
{
    boost::thread t[3];

    for (int i = 0; i < 3; ++i) {
        t[i] = boost::thread(task);
    }

    for (int i = 0; i < 3; ++i) {
        t[i].join();
    }
}
