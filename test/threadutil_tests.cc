/*
 * Repository:  https://github.com/kingkybel/ThreadUtilities
 * File Name:   test/threadutil_tests.cc
 * Description: Unit tests for thread utilities.
 *
 * Copyright (C) 2024 Dieter J Kybelksties <github@kybelksties.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * @date: 2024-12-20
 * @author: Dieter J Kybelksties
 */
#include "threadutil.h"

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace std;
using namespace util;

class ThreadutilTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // just in case we need it
    }

    void TearDown() override
    {
        // just in case we need it
    }
};

double somefunc(int x, double y)
{
    if (y < 0.0)
    {
        throw std::runtime_error("y is negative"); // NOSONAR S112: OK in test
    }
    return static_cast<double>(x) + y;
}

TEST_F(ThreadutilTest, future_with_exception_test)
{
    int    x             = 5;
    double y             = 6.4;
    auto   result_future = make_exception_safe_future(somefunc, x, y);
    double result{};

    ASSERT_NO_THROW(result = result_future.get());
    ASSERT_EQ(result, 11.4);

    y             = -5.0;
    result_future = make_exception_safe_future(somefunc, x, y);
    ASSERT_THROW(result_future.get(), std::exception);
}

TEST_F(ThreadutilTest, future_with_non_standard_exception_test)
{
    auto future = make_exception_safe_future([]() -> int { throw 7; });
    ASSERT_THROW(future.get(), int);
}

TEST_F(ThreadutilTest, thread_function_executes_callable_test)
{
    std::atomic<int>                   observed{0};
    auto                               callable = [&observed]() { observed.store(123); };
    ThreadFunction<decltype(callable)> thread_function{std::move(callable)};

    auto worker = thread_function.start_thread();
    ASSERT_TRUE(worker.joinable());
    worker.join();
    ASSERT_EQ(observed.load(), 123);
}

TEST_F(ThreadutilTest, make_thread_func_ptr_returns_executable_base_test)
{
    std::atomic<int> observed{0};
    auto             thread_func =
        make_thread_func_ptr([](std::atomic<int>& out, int value) { out.store(value); }, std::ref(observed), 77);

    ASSERT_NE(thread_func, nullptr);
    auto worker = thread_func->start_thread();
    ASSERT_TRUE(worker.joinable());
    worker.join();
    ASSERT_EQ(observed.load(), 77);
}

TEST_F(ThreadutilTest, priority_thread_accessors_and_ordering_test)
{
    auto low_priority  = PriorityThread{11, 1, make_thread_func_ptr([]() { /*some body*/ })};
    auto high_priority = PriorityThread{22, 5, make_thread_func_ptr([]() { /*some body*/ })};

    ASSERT_EQ(low_priority.id(), 11);
    ASSERT_EQ(low_priority.priority(), 1);
    low_priority.increase_priority();
    ASSERT_EQ(low_priority.priority(), 2);
    ASSERT_TRUE(low_priority.arrival_time() <= std::chrono::steady_clock::now());
    ASSERT_TRUE(low_priority < high_priority);
}

TEST_F(ThreadutilTest, priority_thread_start_runs_wrapped_function_test)
{
    std::atomic<int> observed{0};
    PriorityThread   priority_thread{
        99,
        2,
        make_thread_func_ptr([](std::atomic<int>& out, int value) { out.store(value); }, std::ref(observed), 42)
    };

    auto worker = priority_thread.start();
    ASSERT_TRUE(worker.joinable());
    worker.join();
    ASSERT_EQ(observed.load(), 42);
}

TEST_F(ThreadutilTest, scheduler_executes_threads_from_queue_test)
{
    std::atomic<int> execution_sum{0};

    ThreadScheduler scheduler{default_priority_intervals, 2};
    scheduler.addThread(1'001, 1, [&execution_sum](int value) { execution_sum.fetch_add(value); }, 3);
    scheduler.addThread(1'002, 3, [&execution_sum](int value) { execution_sum.fetch_add(value); }, 5);

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline && execution_sum.load() != 8)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_EQ(execution_sum.load(), 8);
    scheduler.terminate();
}

TEST_F(ThreadutilTest, scheduler_terminate_without_work_test)
{
    ThreadScheduler scheduler{default_priority_intervals, 1};
    scheduler.terminate();
    SUCCEED();
}

TEST_F(ThreadutilTest, scheduler_executes_multiple_tasks_test)
{
    std::atomic<int> executions{0};

    ThreadScheduler scheduler{default_priority_intervals, 3};
    for (int i = 0; i < 5; ++i)
    {
        scheduler.addThread(static_cast<uint64_t>(2'000 + i), static_cast<uint64_t>(i % 2), [&executions]() {
            executions.fetch_add(1);
        });
    }

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (std::chrono::steady_clock::now() < deadline && executions.load() != 5)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_EQ(executions.load(), 5);
    scheduler.terminate();
}
