/*
 * Repository:  https://github.com/kingkybel/ThreadUtilities
 * File Name:   include/threadutil.h
 * Description: Utility for threads.
 *
 * Copyright (C) 2024 Dieter J Kybelksties
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

#ifndef NS_UTIL_THREADUTIL_H_INCLUDED
#define NS_UTIL_THREADUTIL_H_INCLUDED

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

namespace util
{
struct mutexed_object_base
{
    virtual void do_run() = 0;
    std::mutex   mtx;
};

template <typename... MtxObjT_>
struct deferred_lock_barrier
{
    std::tuple<MtxObjT_...> objs;

    void synchronise()
    {
        std::vector<std::unique_lock<std::mutex>> locks;
        for (auto& i: std::index_sequence_for<MtxObjT_...>())
        {
            locks.emplace_back(std::unique_lock<std::mutex>(std::get<i>(objs).mtx, std::defer_lock));
        }
    }
};

/**
 * @brief future/promise implementation that is thread safe
 *
 * @tparam Func function that may throw
 * @tparam Args variadic argument types
 * @param func function/function object
 * @param args function arguments
 * @return std::future<decltype(func(std::forward<Args>(args)...))> a future that encapsulates the result, including any
 * exception
 */
template <typename Func, typename... Args>
auto make_exception_safe_future(Func&& func, Args&&... args) -> std::future<decltype(func(std::forward<Args>(args)...))>
{
    using ResultType = decltype(func(std::forward<Args>(args)...));
    std::promise<ResultType> promise;
    auto                     future = promise.get_future();

    try
    {
        // Call the provided function and set the result in the promise
        promise.set_value(func(std::forward<Args>(args)...));
    }
    catch (std::exception const& ex)
    {
        // Handle exceptions and set the exception in the promise
        promise.set_exception(std::current_exception());
    }
    catch (...)
    {
        // Handle non-standard exceptions and set them in the promise
        promise.set_exception(std::current_exception());
    }

    return future;
}

/**
 * @brief Base for any thread-function
 */
struct ThreadFuncBase
{
    /**
     * @brief execute the function in a thread (abstract)
     */
    virtual std::thread start_thread() = 0;
};

/**
 * @brief Thread-function wrapper derive from ThreadFuncBase
 *
 * @tparam Func function type
 * @tparam Args args-type variadic for compile-time polymorph functions
 */
template <typename Func, typename... Args>
struct ThreadFunction : public ThreadFuncBase
{
    /**
     * @brief Construct a new Thread Function object given the function (-object) and the parameters
     *
     * @param func function
     * @param args arguments
     */
    explicit ThreadFunction(Func&& func, Args&&... args)
        : func_(func)
        , args_(std::tuple<Args...>(args...))
    {
    }

    /**
     * @brief Create a thread from the given function. This will automatically start the thread.
     *
     * @return std::thread the (started) thread
     */
    std::thread start_thread() override
    {
        return make_thread_(args_, std::index_sequence_for<Args...>());
    }

  private:
    /**
     * @brief Helper to make a variadic list from the tuple again and call the function
     *
     * @tparam Is index sequence
     * @param tuple the tuple to pack
     */
    template <std::size_t... Is>
    void package_tuple_and_call_function_(std::tuple<Args...> const& tuple, std::index_sequence<Is...>)
    {
        func_(std::get<Is>(args_)...);
    }

    /**
     * @brief Helper to make a variadic list from the tuple again and use that to create a thread from the function and
     * arguments.
     *
     * @tparam Is index sequence
     * @param tuple the tuple to pack
     */
    template <std::size_t... Is>
    std::thread make_thread_(std::tuple<Args...> const& tuple, std::index_sequence<Is...>)
    {
        return std::thread{func_, std::get<Is>(args_)...};
    }

    typedef std::function<void(Args...)> FuncType;

    FuncType            func_;
    std::tuple<Args...> args_;
};

/**
 * @brief Create a pointer to ThreadFuncBase from a concrete, compile-time polymorph ThreadFunction
 *
 * @tparam Func_ function type
 * @tparam Args_ variadic argument types
 * @param func function to serve as thread function
 * @param args arguments for the thread function
 * @return std::shared_ptr<ThreadFuncBase> shared pointer to the thread-function
 */
template <typename Func_, typename... Args_>
std::shared_ptr<ThreadFuncBase> make_thread_func_ptr(Func_ func, Args_... args)
{
    auto pFunc = std::make_shared<ThreadFunction<Func_, Args_...>>(std::move(func), std::move(args)...);

    return std::dynamic_pointer_cast<ThreadFuncBase>(pFunc);
}

namespace
{
using millis                    = std::chrono::milliseconds;
auto default_priority_intervals = std::vector<millis>{millis{50}, millis{200}, millis{500}, millis{1'000}};
}; // namespace

/**
 * @brief Priority thread wrapper.
 * Contains:
 * <ul>
 *  <li>an ID</li>
 *  <li>a priority</li>
 *  <li>a pointer to a (polymorph) function to be executed by the thread</li>
 * </ul>
 */
struct PriorityThread
{
    PriorityThread(uint64_t id, uint64_t priority, std::shared_ptr<ThreadFuncBase> pThreadFunc);

    /**
     * @brief Less-operator for PriorityTreads
     *
     * @param lhs left-hand-side
     * @param rhs right-hand-side
     * @return true, if lhs has lower priority than rhs, false otherwise
     */
    friend bool operator<(PriorityThread const& lhs, PriorityThread const& rhs)
    {
        return lhs.priority_ < rhs.priority_;
    }

    /**
     * @brief Retrieve the ID.
     *
     * @return uint64_t the ID
     */
    [[nodiscard]] uint64_t id() const
    {
        return id_;
    }

    /**
     * @brief Retrieve the current priority.
     *
     * @return uint64_t the current priority
     */
    [[nodiscard]] uint64_t priority() const
    {
        return priority_;
    }

    /**
     * @brief Increase the priority.
     */
    void increase_priority()
    {
        priority_++;
    }

    /**
     * @brief Retrieve the arrival-time of this PriorityThread.
     *
     * @return chrono::steady_clock::time_point the arrival-time
     */
    [[nodiscard]] std::chrono::steady_clock::time_point arrival_time() const
    {
        return arrival_time_;
    }

    std::thread start()
    {
        return pThreadFunc_->start_thread();
    }

  private:
    uint64_t                              id_;
    uint64_t                              priority_;
    std::chrono::steady_clock::time_point arrival_time_;
    std::shared_ptr<ThreadFuncBase>       pThreadFunc_;
};

/**
 * @brief Schedule threads for execution according to priority.
 * Limit the number of parallel threads to the given pool_size.
 * According to given priority_intervals increase priority of threads
 * that have waited a long time.
 */
class ThreadScheduler
{
  public:
    explicit ThreadScheduler(
        std::vector<millis> const& priority_intervals = default_priority_intervals,
        uint64_t pool_size                            = std::thread::hardware_concurrency() * 2
    );

    ~ThreadScheduler();

    /**
     * @brief Terminate the thread-scheduler.
     */
    void terminate();

    /**
     * @brief Add a thread to the priority-queue
     *
     * @tparam Func function type
     * @tparam Args function arg-types (variadic)
     * @param id thread id
     * @param priority initial priority of the thread
     * @param func thread function
     * @param args arguments for the thread function
     */
    template <typename Func_, typename... Args_>
    void addThread(uint64_t id, uint64_t priority, Func_&& func, Args_&&... args)
    {
        PriorityThread priority_thread{
            id,
            priority,
            make_thread_func_ptr(std::forward<Func_>(func), std::forward<Args_>(args)...)
        };
        {
            std::unique_lock<std::mutex> lock(mutex_);
            priority_thread_queue_.push(priority_thread);
        }
        // tell everyone that we have an element in the queue
        cv_.notify_one();
    }

  private:
    void        processQueueThread();
    std::thread processQueue();

    std::priority_queue<PriorityThread> priority_thread_queue_;
    std::vector<millis>                 priority_intervals_;
    uint64_t                            pool_size_;
    std::mutex                          mutex_;
    std::condition_variable             cv_;
    std::thread                         queue_processor_thread_;
    bool volatile terminate_ = false;
};

// using namespace std;
// int main()
// {
//     auto scheduler = ThreadScheduler{default_priority_intervals, std::thread::hardware_concurrency()};
//
//     for(size_t i = 0; i < 20; i++)
//         scheduler.addThread(
//          4711 + i,
//          0 % 5,
//          [](int id)
//          {
//              cout << "threadID=" << std::this_thread::get_id() << ": Hello from " << id << endl;
//              this_thread::sleep_for(millis{1500 * (id % 3)});
//              cout << "threadID=" << std::this_thread::get_id() << ": finished " << id <<endl;
//          },
//          i);
//
//     cout << "main-threadID=" << std::this_thread::get_id() << endl;
//     this_thread::sleep_for(std::chrono::seconds{5});
//
//     scheduler.terminate();
//
//     return 0;
// }

}; // namespace util

#endif // NS_UTIL_THREADUTIL_H_INCLUDED
