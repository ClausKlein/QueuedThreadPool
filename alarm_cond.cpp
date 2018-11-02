/*
 * alarm_cond.cpp
 *
 * This is an enhancement to the alarm_mutex.c program, which used only a mutex
 * to synchronize access to the shared alarm list. This version adds a
 * condition variable. The alarm thread waits on this condition variable, with
 * a timeout that corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable so that the
 * alarm thread will wake up and process the earlier timeout first, requeueing
 * the later request.
 *
 * CHANGELOG:
 *  ported to C++11; ck
 *  use steady_clock instead of system_clock! ck
 *  use strstream instead of stdio! ck
 */

#include <stdlib.h> // exit()

#include <cassert> // assert()
#include <iostream>
#include <sstream>
#include <string>


#if defined(__cplusplus) && (__cplusplus >= 201103L)
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using std::chrono::duration;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;
using std::chrono::time_point;
using namespace std;

#else //##########################################

#include <boost/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

using boost::chrono::duration;
using boost::chrono::seconds;
using boost::chrono::steady_clock;
using boost::chrono::system_clock;
using boost::chrono::time_point;
using namespace boost;
#endif //##########################################


#include <boost/algorithm/string/trim.hpp>
#include <boost/current_function.hpp>


#ifdef NDEBUG
#define TRACE(x)
#else
static void TRACE(const std::string& msg)
{
    static mutex _mut; // private mutex only!
    lock_guard<mutex> lk(_mut);
    std::cerr << msg << std::endl;
}
#else
#define DEBUG
#endif


namespace Alarm
{
/*
 * The "alarm" structure now contains the time_t (time since the Epoch, in
 * seconds) for each alarm, so that they can be sorted. Storing the requested
 * number of seconds would not be enough, since the "alarm thread" cannot tell
 * how long it has been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag* link;
    time_t seconds;
    time_point<steady_clock> time;
    std::string message;
} alarm_t;
}


//==================================
// shared data, mutex, and condition
mutex alarm_mutex;
condition_variable alarm_cond;
Alarm::alarm_t* alarm_list = NULL;
time_point<steady_clock> current_alarm(seconds(0LL));
const time_point<steady_clock> ZERO(seconds(0LL));
bool enabled = true;
//==================================


/*
 * Insert alarm entry on list, in order of time.
 */
static time_point<steady_clock> alarm_insert(Alarm::alarm_t* alarm)
{
    static Alarm::alarm_t* longest(NULL);
    Alarm::alarm_t **last, *next;

    assert(alarm != NULL);

    /*
     * LOCKING PROTOCOL:
     *
     * This routine requires that the caller have locked the alarm_mutex!
     */
    last = &alarm_list;
    next = *last;
    while (next != NULL) {
        if (alarm->time < next->time) {
            alarm->link = next;
            *last       = alarm;
            break;
        }
        // insert in alarm list at current position
        last = &next->link;
        next = next->link;
    }

    /*
     * If we reached the end of the list, insert the new alarm there.  ("next"
     * is NULL, and "last" points to the link field of the last item, or to the
     * list header.)
     */
    if (next == NULL) {
        *last       = alarm;
        alarm->link = NULL;
        longest     = alarm; // NOTE: this is the last alarm for now!
    }

#ifdef DEBUG
    {
        std::ostringstream os;
        Alarm::alarm_t* longestAlarm = alarm_list;
        os << "alarm_list = [";
        for (next = alarm_list; next != NULL; next = next->link) {
            longestAlarm                 = next;
            time_point<steady_clock> now = steady_clock::now();
            duration<double> timeleft    = next->time - now;
            os << next->seconds << "(" << timeleft.count() << ")\""
               << next->message << "\"" << std::endl;
        }
        os << "]";
        TRACE(os.str());
        assert(longestAlarm == longest);
    }
#endif

    /*
     * Wake the alarm thread if it is not busy (that is, if current_alarm is
     * ZERO, signifying that it's waiting for work), or if the new alarm comes
     * before the one on which the alarm thread is waiting.
     */
    if ((current_alarm == ZERO) || (alarm->time < current_alarm)) {
        current_alarm = alarm->time;
        alarm_cond.notify_one();
    }

    assert(longest != NULL);
    return longest->time;
}


/*
 * The alarm thread's start routine.
 */
void alarm_thread(void)
{
    Alarm::alarm_t* alarm;
    time_point<steady_clock> now;
    bool expired;

    /*
     * Loop forever, processing commands. The alarm thread will be
     * disintegrated when the process exits. Lock the mutex at the start -- it
     * will be unlocked during condition waits, so the main thread can insert
     * alarms.
     */
    unique_lock<mutex> lk(alarm_mutex);

    // while event dispatcher loop
    do {
        /*
         * If the alarm list is empty, wait until an alarm is added. Setting
         * current_alarm to ZERO informs the insert routine that the thread is
         * not busy.
         */
        current_alarm = ZERO;
        while (enabled && alarm_list == NULL) {
            alarm_cond.wait(lk);
        }
        if (!enabled) {
            break;
        }

        // get first alarm from alarm list
        alarm      = alarm_list;
        alarm_list = alarm->link;

        now     = steady_clock::now();
        expired = false;
        if (alarm->time > now) {
            duration<double> timeleft = alarm->time - now;

#ifdef DEBUG
            std::ostringstream os;
            os << "[waiting: " << alarm->seconds << "(" << timeleft.count()
               << ")\"" << alarm->message << "\"]" << std::endl;
            TRACE(os.str());
#endif

            current_alarm = alarm->time;
            while (enabled && alarm->time == current_alarm) {
                // use system_clock: cv_status status =
                // alarm_cond.wait_until(lk, current_alarm);
                cv_status status =
                    alarm_cond.wait_for(lk, timeleft); // use steady_clock
                if (status == cv_status::timeout) {
                    expired = true;
                    break;
                }
            }
            if (!enabled) {
                break;
            }

            // current_alarm has changed
            if (!expired) {
                alarm_insert(alarm);
            }
        } else {
            expired = true;
        }

        // handle alarm
        if (expired) {
            std::cerr << "handle(" << alarm->seconds << ") " << alarm->message
                      << std::endl;
            delete (alarm);
        }

    } while (enabled);
    TRACE(
        std::string(BOOST_CURRENT_FUNCTION) + " stopped"); // use concatenation
}


int main(int arc, char** argv)
{
    std::string line;
    Alarm::alarm_t* alarm; // TODO use shared ptr! CK
    time_point<steady_clock> last = ZERO;

    thread scheduler(&alarm_thread);

    while (enabled) {
        std::cout << "Alarm> ";
        if (!std::getline(std::cin, line)) { // EOF
            if (arc > 1 && std::string(argv[1]) == "--wait") {
                time_point<steady_clock> now = steady_clock::now();
                duration<double> timeleft    = last - now;
                std::cerr << "terminate in " << timeleft.count() << "sec ..."
                          << std::endl;
                // wait before terminate to finish test ...
                this_thread::sleep_for(timeleft);
            }
            //=================================
            unique_lock<mutex> lk(alarm_mutex);
            enabled = false;
            alarm_cond.notify_all();
            lk.unlock();
            //=================================
            scheduler.join();
            break;
        }
        if (line.length() <= 3) {
            std::cerr << "Alarm format: [seconds message] | <EOF>"
                      << std::endl;
            continue; // too short ...
        }

        alarm = new (Alarm::alarm_t);

        /*
         * Parse input line into seconds and a message [^\n]),
         * separated from the seconds by whitespace.
         */
        std::istringstream input(line);
        input >> alarm->seconds;
        std::getline(input, alarm->message);
        boost::algorithm::trim(alarm->message);
        if (!input) {
            std::cerr << "Alarm format: [seconds message] | <EOF>"
                      << std::endl;
            delete (alarm);
        } else {
            //=================================
            unique_lock<mutex> lk(alarm_mutex);
            alarm->time = steady_clock::now() + seconds(alarm->seconds);
            TRACE(line);
            /*
             * Insert the new alarm into the list of alarms, sorted by
             * expiration time.
             */
            last = alarm_insert(alarm);
            //=================================
        }
    } // end while input

    exit(EXIT_SUCCESS);
}

// test with:
// tail -f alarm_cond.txt | ./alarm_cond
// cat alarm_cond.txt | ./alarm_cond --wait
//
