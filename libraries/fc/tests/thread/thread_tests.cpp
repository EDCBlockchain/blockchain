
#include <boost/test/unit_test.hpp>

#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/asio.hpp>

#include <iostream>

using namespace fc;

BOOST_AUTO_TEST_SUITE(thread_tests)

BOOST_AUTO_TEST_CASE(executes_task)
{
    bool called = false;
    fc::thread thread("my");
    thread.async([&called]{called = true;}).wait();
    BOOST_CHECK(called);
}

BOOST_AUTO_TEST_CASE(returns_value_from_function)
{
    fc::thread thread("my");
    BOOST_CHECK_EQUAL(10, thread.async([]{return 10;}).wait());
}

BOOST_AUTO_TEST_CASE(executes_multiple_tasks)
{
    bool called1 = false;
    bool called2 = false;

    fc::thread thread("my");
    auto future1 = thread.async([&called1]{called1 = true;});
    auto future2 = thread.async([&called2]{called2 = true;});

    future2.wait();
    future1.wait();

    BOOST_CHECK(called1);
    BOOST_CHECK(called2);
}

BOOST_AUTO_TEST_CASE(calls_tasks_in_order)
{
    std::string result;

    fc::thread thread("my");
    auto future1 = thread.async([&result]{result += "hello ";});
    auto future2 = thread.async([&result]{result += "world";});

    future2.wait();
    future1.wait();

    BOOST_CHECK_EQUAL("hello world", result);
}

BOOST_AUTO_TEST_CASE(yields_execution)
{
    std::string result;

    fc::thread thread("my");
    auto future1 = thread.async([&result]{fc::yield(); result += "world";});
    auto future2 = thread.async([&result]{result += "hello ";});

    future2.wait();
    future1.wait();

    BOOST_CHECK_EQUAL("hello world", result);
}

BOOST_AUTO_TEST_CASE(quits_infinite_loop)
{
    fc::thread thread("my");
    auto f = thread.async([]{while (true) fc::yield();});

    thread.quit();
    BOOST_CHECK_THROW(f.wait(), fc::canceled_exception);
}

BOOST_AUTO_TEST_CASE(reschedules_yielded_task)
{
    int reschedule_count = 0;

    fc::thread thread("my");
    auto future = thread.async([&reschedule_count]
            {
                while (reschedule_count < 10)
                {
                    fc::yield(); 
                    reschedule_count++;
                }
            });

    future.wait();
    BOOST_CHECK_EQUAL(10, reschedule_count);
}

/****
 * Attempt to have fc::threads use fc::mutex when yield() causes a context switch
 */
BOOST_AUTO_TEST_CASE( yield_with_mutex )
{ 
   // set up thread pool
   uint16_t num_threads = 5;
   std::vector<fc::thread*> thread_collection;
   for(uint16_t i = 0; i < num_threads; i++)
      thread_collection.push_back(new fc::thread("My" + std::to_string(i)));

   // the function that will give a thread something to do
   fc::mutex my_mutex;
   volatile uint32_t my_mutable = 0;
   auto my_func = ([&my_mutable, &my_mutex] ()
         {
            // grab the mutex
            my_mutex.lock();
            // get the prior value
            uint32_t old_value = my_mutable;
            // modify the value
            my_mutable++;
            // yield
            fc::yield();
            // test to see if the mutex is recursive
            my_mutex.lock();
            // return the value to the original
            my_mutable--;
            my_mutex.unlock();
            // verify the original still matches
            if (old_value != my_mutable)
               BOOST_FAIL("Values do not match");
            my_mutex.unlock();
         });

   // the loop that gives threads the work
   uint16_t thread_counter = 0;
   uint32_t num_loops = 50000;
   std::vector<fc::future<void>> futures(num_loops);
   for(uint32_t i = 0; i < num_loops; ++i)
   { 
      futures[i] = thread_collection[thread_counter]->async(my_func);
      ++thread_counter;
      if (thread_counter == num_threads)
         thread_counter = 0;
   }

   // now wait for each to finish
   for(uint32_t i = 0; i < num_loops; ++i)
      futures[i].wait();

   // clean up the thread pointers
   for(uint16_t i = 0; i < num_threads; i++)
      delete thread_collection[i];

   // verify that evertying worked
   BOOST_CHECK_EQUAL(0u, my_mutable);
}

BOOST_AUTO_TEST_SUITE_END()
