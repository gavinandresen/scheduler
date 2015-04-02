#include "scheduler.h"

#include <boost/bind.hpp>
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

int main(int argc, char** argv)
{
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
