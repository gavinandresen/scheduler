#include "scheduler.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread.hpp>
#include <iostream>

void printEleven()
{
    std::cout << "Eleven!\n";
}

void printSomething(std::string message)
{
    std::cout << message << "\n";
}

void repeatStuff(CScheduler& s)
{
    std::cout << "Gonna start repeating every 2 secs\n";
    s.scheduleEvery(boost::bind(printSomething, std::string("Repeat!")), 2);
}

void longRunningTask(int nSecondsToWait)
{
    std::cout << "Long-running task, gonna sleep for " << nSecondsToWait << "seconds\n";
    boost::this_thread::sleep_for(boost::chrono::seconds(nSecondsToWait));
    std::cout << "Done sleeping\n";
}

void microTask(CScheduler& s, boost::mutex& mutex, int& counter, int delta, boost::chrono::system_clock::time_point rescheduleTime)
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        counter += delta;
    }
    boost::chrono::system_clock::time_point noTime = boost::chrono::system_clock::time_point::min();
    if (rescheduleTime != noTime) {
        CScheduler::Function f = boost::bind(&microTask, boost::ref(s), boost::ref(mutex), boost::ref(counter), -delta + 1, noTime);
        s.schedule(f, rescheduleTime);
    }
}

int main(int argc, char** argv)
{
    // Stress test: thousands of microsecond-scheduled tasks,
    // serviced by 100 threads.
    //
    // So... ten shared counters, which if all the tasks execute
    // properly will be zero when the dust settles.
    // Each task adds or subtracts from one of the counters a
    // random amount, and then schedules another task 0-1000
    // microseconds in the future to subtract or add from
    // the counter -random_amount+2, so in the end the shared
    // counters should sum to the number of tasks performed.
    CScheduler microTasks;

    boost::thread_group microThreads;
    for (int i = 0; i < 50; i++)
        microThreads.create_thread(boost::bind(&CScheduler::ServiceQueue, &microTasks));

    boost::mutex counterMutex[10];
    int counter[10] = {0};
    boost::random::mt19937 rng(argc); // Seed with number of arguments
    boost::random::uniform_int_distribution<> zeroToNine(0, 9);
    boost::random::uniform_int_distribution<> randomMsec(-11, 1000);
    boost::random::uniform_int_distribution<> randomDelta(-1000, 1000);

    boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
    boost::chrono::system_clock::time_point now = start;

    for (int i = 0; i < 1000; i++) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randomMsec(rng));
        boost::chrono::system_clock::time_point tReschedule = now + boost::chrono::microseconds(500 + randomMsec(rng));
        int whichCounter = zeroToNine(rng);
        CScheduler::Function f = boost::bind(&microTask, boost::ref(microTasks),
                                             boost::ref(counterMutex[whichCounter]), boost::ref(counter[whichCounter]),
                                             randomDelta(rng), tReschedule);
        microTasks.schedule(f, t);
    }
    boost::this_thread::sleep_for(boost::chrono::microseconds(600));
    now = boost::chrono::system_clock::now();
    // More threads and more tasks:
    for (int i = 0; i < 50; i++)
        microThreads.create_thread(boost::bind(&CScheduler::ServiceQueue, &microTasks));
    for (int i = 0; i < 1000; i++) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randomMsec(rng));
        boost::chrono::system_clock::time_point tReschedule = now + boost::chrono::microseconds(500 + randomMsec(rng));
        int whichCounter = zeroToNine(rng);
        CScheduler::Function f = boost::bind(&microTask, boost::ref(microTasks),
                                             boost::ref(counterMutex[whichCounter]), boost::ref(counter[whichCounter]),
                                             randomDelta(rng), tReschedule);
        microTasks.schedule(f, t);
    }

    // All 2,000 tasks should be finished within 2 milliseconds. Sleep a bit longer.
    boost::this_thread::sleep_for(boost::chrono::microseconds(2100));

    microThreads.interrupt_all();
    microThreads.join_all();

    std::cout << "Microtask counts: ";
    int counterSum = 0;
    for (int i = 0; i < 10; i++) {
        std::cout << counter[i] << " ";
        counterSum += counter[i];
    }
    std::cout << "\nSum: " << counterSum << "\n";
    assert(counterSum == 2000);

    //
    // Human-timescale tests
    //
    CScheduler s;

    s.scheduleFromNow(printEleven, 11);
    s.scheduleFromNow(boost::bind(printSomething, std::string("Two")), 2);
    s.scheduleFromNow(boost::bind(printSomething, std::string("One")), 1);
    s.scheduleFromNow(boost::bind(printSomething, std::string("Five")), 5);
    s.scheduleFromNow(boost::bind(printSomething, std::string("Wait... negative?")), -1);
    s.scheduleFromNow(boost::bind(printSomething, std::string("AlsoTwo")), 2);
    s.scheduleFromNow(boost::bind(printSomething, std::string("Three")), 3);
    // After 4 seconds, start repeating every two seconds:
    CScheduler::Function repeatFunction = boost::bind(repeatStuff, boost::ref(s));
    s.scheduleFromNow(repeatFunction, 4);

    boost::thread* schedulerThread = new boost::thread(boost::bind(&CScheduler::ServiceQueue, &s));
    boost::this_thread::sleep_for(boost::chrono::seconds(12));

    schedulerThread->interrupt();
    try {
        schedulerThread->join();
    } catch (const boost::thread_interrupted&) {
        // this is normal.
    }
    delete schedulerThread;

    // Note: even though the thread terminated, the
    // repeatStuff task is still on the queue.

    // Now use two threads to service the queue.
    // If you change this code to use just one thread,
    // the longRunningTask will prevent the other tasks
    // from running.

    s.scheduleFromNow(boost::bind(printSomething, std::string("Two")), 2);
    s.scheduleFromNow(boost::bind(printSomething, std::string("One")), 1);
    s.scheduleFromNow(boost::bind(longRunningTask, 11), 0);

    boost::thread_group threadGroup;
    threadGroup.create_thread(boost::bind(&CScheduler::ServiceQueue, &s));
    threadGroup.create_thread(boost::bind(&CScheduler::ServiceQueue, &s));
    boost::this_thread::sleep_for(boost::chrono::seconds(13));

    // All threads MUST be terminated before the scheduler's
    // destructor is called.
    threadGroup.interrupt_all();
    threadGroup.join_all();

    return 0;
}
