// see [N4045] Kohlhoff, Christopher, Library Foundations for Asynchronous
// Operations, Revision 2, 2014. and [N4242] Kohlhoff, Christopher, Executors
// and Asynchronous Operations, Revision 2, 2015.
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/p0113r0.html
// https://think-async.com/Asio/Documentation
// https://github.com/chriskohlhoff/asio/

#define ASIO_NO_DEPRECATED
#define ASIO_HAS_STD_FUTURE

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/ts/executor.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>

#include <ctime>
#include <exception>
#include <iostream>

#if defined(ASIO_HAS_STD_FUTURE)
#include <future>
#endif

using boost::asio::bind_executor;
using boost::asio::post;
using boost::asio::thread_pool;
using boost::asio::use_future;


// Run a (lambda) function asynchronously and wait for the result:
int main()
{
    try {
        thread_pool pool;

#if defined(ASIO_HAS_STD_FUTURE)
        std::future<int> f = post(pool, use_future([] {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
            // ...
            if (std::time(NULL) % 2) {
                throw std::runtime_error("Sorry, no answer at this time!");
            }
            return 42;
        }));

        std::cout << "Wait something ..., result = " << std::flush << f.get()
                  << std::endl;
#else
        throw std::runtime_error("Sorry, no future => no answer!");
#endif

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
