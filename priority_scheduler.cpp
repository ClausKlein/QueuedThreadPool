#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
#endif

#include <asio/dispatch.hpp>
#include <asio/execution_context.hpp>

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <iostream>
#include <memory>
#include <queue>

using asio::dispatch;
using asio::execution_context;


class priority_scheduler : public execution_context {
public:
    // A class that satisfies the Executor requirements.
    class executor_type {
    public:
        executor_type(priority_scheduler& ctx, int pri) ASIO_NOEXCEPT
            : context_(ctx),
              priority_(pri)
        {
            context_.stopped_ = false;
        }

        priority_scheduler& context() const ASIO_NOEXCEPT { return context_; }

        void on_work_started() const ASIO_NOEXCEPT
        {
            // This executor doesn't count work. Instead, the scheduler simply
            // runs until explicitly stopped.
        }

        void on_work_finished() const ASIO_NOEXCEPT
        {
            // This executor doesn't count work. Instead, the scheduler simply
            // runs until explicitly stopped.
        }

        template <class Func, class Alloc>
        // TODO: NO rvalue! void dispatch(Func&& f, const Alloc& a) const
        void dispatch(ASIO_MOVE_ARG(Func) f, const Alloc& a) const
        {
            post(boost::forward<Func>(f), a);
        }

        template <class Func, class Alloc>
        void post(Func f, const Alloc& a) const
        {

#if defined(__cplusplus) && (__cplusplus >= 201103L)
            auto p(boost::allocate_shared<item<Func> >(
                typename std::allocator_traits<Alloc>::template rebind_alloc<
                    char>(a),
                priority_, boost::move(f)));
#else
            boost::shared_ptr<item_base> p(boost::allocate_shared<item<Func> >(
                std::allocator<char>(a), priority_, boost::move(f)));
#endif

            std::lock_guard<boost::mutex> lock(context_.mutex_);
            context_.queue_.push(p);
            context_.condition_.notify_one();
        }

        template <class Func, class Alloc>
        // TODO: NO rvalue! void defer(Func&& f, const Alloc& a) const
        void defer(ASIO_MOVE_ARG(Func) f, const Alloc& a) const
        {
            post(boost::forward<Func>(f), a);
        }

        friend bool operator==(
            const executor_type& a, const executor_type& b) ASIO_NOEXCEPT
        {
            return &a.context_ == &b.context_;
        }

        friend bool operator!=(
            const executor_type& a, const executor_type& b) ASIO_NOEXCEPT
        {
            return &a.context_ != &b.context_;
        }

    private:
        priority_scheduler& context_;
        int priority_;
    };

    executor_type get_executor(int pri = 0) ASIO_NOEXCEPT
    {
        return executor_type(*const_cast<priority_scheduler*>(this), pri);
    }

    void run()
    {
        boost::unique_lock<boost::mutex> lock(mutex_);
        for (;;) {

#if defined(__cplusplus) && (__cplusplus >= 201103L)
            condition_.wait(lock, [&] { return hasWork(); });
#else
            while (!hasWork()) {
                condition_.wait(lock);
            }
#endif

            if (stopped_) {
                return;
            }

            // NOTE: prevent auto p(queue_.top());
            boost::shared_ptr<item_base> p(queue_.top());
            queue_.pop();
            lock.unlock();
            p->execute_(p);
            lock.lock();
        }
    }

    void stop()
    {
        std::lock_guard<boost::mutex> lock(mutex_);
        stopped_ = true;
        condition_.notify_all();
    }

private:
    inline bool hasWork() { return stopped_ || !queue_.empty(); }

    struct item_base {
        int priority_;
        void (*execute_)(boost::shared_ptr<item_base>&);
    };

    template <class Func> struct item : item_base {
        item(int pri, Func f)
            : function_(boost::move(f))
        {
            priority_ = pri;
            // FIXME: no lambda fct! CK
            // XXX execute_ = [](boost::shared_ptr<item_base>& p)
            execute_ = execute;
        }

        static void execute(boost::shared_ptr<item_base>& p)
        {
            Func tmp(boost::move(static_cast<item*>(p.get())->function_));
            p.reset();
            tmp();
        };

        Func function_;
    };

    struct item_comp {
        bool operator()(const boost::shared_ptr<item_base>& a,
            const boost::shared_ptr<item_base>& b)
        {
            return a->priority_ < b->priority_;
        }
    };

    boost::mutex mutex_;
    boost::condition_variable condition_;
    std::priority_queue<boost::shared_ptr<item_base>,
        std::vector<boost::shared_ptr<item_base> >, item_comp>
        queue_;
    bool stopped_;
};


class Print {
    int i_;

public:
    Print(int i)
        : i_(i) {};
    void operator()() { std::cout << i_ << std::endl; }
};


int main()
{
    priority_scheduler sched;

#if defined(__cplusplus) && (__cplusplus >= 201103L)
    auto low  = sched.get_executor(0);
    auto med  = sched.get_executor(1);
    auto high = sched.get_executor(2);

    dispatch(low, [] { std::cout << "1\n"; });
    dispatch(low, [] { std::cout << "11\n"; });
    dispatch(med, [] { std::cout << "2\n"; });
    dispatch(med, [] { std::cout << "22\n"; });
    dispatch(high, [] { std::cout << "3\n"; });
    dispatch(high, [] { std::cout << "33\n"; });
    dispatch(high, [] { std::cout << "333\n"; });

    dispatch(sched.get_executor(-1), [&] { sched.stop(); });
#else
    priority_scheduler::executor_type low = sched.get_executor(0);
    priority_scheduler::executor_type med = sched.get_executor(1);
    priority_scheduler::executor_type high = sched.get_executor(2);

    dispatch(low, Print(1));
    dispatch(low, Print(11));
    dispatch(med, Print(2));
    dispatch(med, Print(22));
    dispatch(high, Print(3));
    dispatch(high, Print(33));
    dispatch(high, Print(333));

    dispatch(sched.get_executor(-1),
        boost::bind(&priority_scheduler::stop, &sched));
#endif

    sched.run();
}
