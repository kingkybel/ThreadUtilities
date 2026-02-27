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
#include <compare>
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
/**
 * @brief Abstract base class for objects that manage a mutex and provide a run operation.
 *
 * This struct serves as an interface for objects that require thread-safe access via a mutex.
 * Derived classes must implement the do_run() virtual method to define the action to be performed.
 */
struct mutexed_object_base
{
    /**
     * @brief Execute the object's operation (abstract).
     * Derived classes must implement this method.
     */
    virtual void do_run() = 0;

    /**
     * @brief Mutex used to synchronize access to the object.
     */
    std::mutex   mtx;
};

/**
 * @brief Utility for deferring locks across multiple mutex-managed objects.
 *
 * This template struct manages a tuple of objects that each contain a mutex.
 * It provides a synchronise() method to acquire deferred locks on all contained objects' mutexes.
 *
 * @tparam MtxObjT_ Variadic template parameter types representing mutex-managed objects
 *                   (each should have a public std::mutex member named 'mtx')
 */
template <typename... MtxObjT_>
struct deferred_lock_barrier
{
    /**
     * @brief Tuple containing the mutex-managed objects to be synchronized.
     */
    std::tuple<MtxObjT_...> objs;

    /**
     * @brief Create deferred locks for all objects' mutexes without acquiring them.
     *
     * This method establishes deferred locks (std::defer_lock) for each mutex in the contained objects.
     * Deferred locks are not acquired immediately but can be locked later using lock() or other synchronization methods.
     */
    void synchronise()
    {
        std::vector<std::unique_lock<std::mutex>> locks;
        for (auto& i: std::index_sequence_for<MtxObjT_...>())
        {
            locks.emplace_back(std::unique_lock(std::get<i>(objs).mtx, std::defer_lock));
        }
    }
};

/**
 * @brief Create a thread-safe future that captures function results or exceptions.
 *
 * This function executes the given callable and returns a future that encapsulates the result.
 * If the function throws an exception, the exception is safely propagated through the future
 * and can be retrieved when calling future.get().
 *
 * @tparam Func Type of the callable object (function, lambda, or functor)
 * @tparam Args Variadic parameter types for the function arguments
 * @param func A callable object to execute
 * @param args Zero or more arguments to pass to the callable
 * @return std::future<decltype(func(std::forward<Args>(args)...))> A future that will contain
 *         either the function's result or any exception it throws
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
    catch (std::exception const& ex) // NOSONAR S1181: Catching all exceptions to ensure they are safely propagated via std::promise
    {
        // Handle exceptions and set the exception in the promise
        promise.set_exception(std::current_exception());
    }
    catch (...) // NOSONAR S1181, S2738: Catching all exceptions to ensure they are safely propagated via std::promise
    {
        // Handle non-standard exceptions and set them in the promise
        promise.set_exception(std::current_exception());
    }

    return future;
}

/**
 * @brief Abstract base class for polymorphic thread functions.
 *
 * Provides an interface for creating and starting threads with various function signatures.
 * Derived classes wrap specific function types and arguments, enabling compile-time polymorphism
 * of thread functions through runtime polymorphic pointers.
 */
struct ThreadFuncBase
{
    /**
     * @brief Create and start a thread with the wrapped function and arguments.
     *
     * @return std::jthread A joinable thread executing the wrapped function
     */
    virtual std::jthread start_thread() = 0;
};

/**
 * @brief Concrete thread-function wrapper implementing ThreadFuncBase.
 *
 * This template struct wraps a callable object (function, lambda, or functor) along with its arguments.
 * It enables passing compile-time polymorphic functions through a runtime polymorphic interface.
 *
 * @tparam Func Type of the callable object (function pointer, lambda, or functor)
 * @tparam Args Variadic parameter types for the function arguments
 */
template <typename Func, typename... Args>
struct ThreadFunction : public ThreadFuncBase
{
    /**
     * @brief Construct a new ThreadFunction with the given callable and arguments.
     *
     * @param func A callable object (function, lambda, or functor)
     * @param args Zero or more arguments to pass to the function
     */
    explicit ThreadFunction(Func&& func, Args&&... args)
        : func_(std::move(func))
        , args_(std::tuple<Args...>(std::move(args)...))
    {
    }

    /**
     * @brief Create and start a thread executing the wrapped function with stored arguments.
     *
     * @return std::jthread A joinable thread that executes the wrapped function
     */
    std::jthread start_thread() override
    {
        return make_thread_(args_, std::index_sequence_for<Args...>());
    }

  private:
    /**
     * @brief Helper to make a variadic list from the tuple and use that to create a thread from the function and
     * arguments.
     *
     * @tparam Is index sequence
     */
    template <std::size_t... Is>
    std::jthread make_thread_([[maybe_unused]] std::tuple<Args...> const&, std::index_sequence<Is...>)
    {
        return std::jthread{func_, std::get<Is>(args_)...};
    }

    using FuncType = std::function<void(Args...)>;

    FuncType                         func_;
    [[no_unique_address]] std::tuple<Args...> args_;
};

/**
 * @brief Create a polymorphic pointer from a concrete thread function with arguments.
 *
 * This helper function wraps a callable object and its arguments in a ThreadFunction instance,
 * then casts it to a ThreadFuncBase pointer for runtime polymorphic use. This allows
 * compile-time polymorphic functions to be used through a uniform runtime polymorphic interface.
 *
 * @tparam Func_ Type of the callable object (function, lambda, or functor)
 * @tparam Args_ Variadic parameter types for the function arguments
 * @param func A callable object to wrap
 * @param args Zero or more arguments to pass to the callable
 * @return std::shared_ptr<ThreadFuncBase> A shared pointer to the wrapped function,
 *         suitable for use with the thread scheduler
 */
template <typename Func_, typename... Args_>
std::shared_ptr<ThreadFuncBase> make_thread_func_ptr(Func_ func, Args_... args)
{
    auto pFunc = std::make_shared<ThreadFunction<Func_, Args_...>>(std::move(func), std::move(args)...);

    return std::dynamic_pointer_cast<ThreadFuncBase>(pFunc);
}

namespace detail
{
using millis                    = std::chrono::milliseconds;
auto const default_priority_intervals = std::vector<millis>{millis{50}, millis{200}, millis{500}, millis{1'000}};
}; // namespace detail

using detail::millis;
using detail::default_priority_intervals;

/**
 * @brief Wrapper for a thread with associated priority and metadata.
 *
 * This struct encapsulates a thread function along with scheduling information:
 * <ul>
 *  <li>A unique identifier for the thread</li>
 *  <li>A priority value used for scheduling decisions</li>
 *  <li>The timestamp when the thread entered the queue</li>
 *  <li>A pointer to the polymorphic function to be executed</li>
 * </ul>
 *
 * PriorityThreads are compared and ordered by priority for use in priority queues.
 */
struct PriorityThread
{
    /**
     * @brief Construct a new PriorityThread with ID, initial priority, and function.
     *
     * @param id Unique identifier for this thread
     * @param priority Initial priority value (higher values = higher priority)
     * @param pThreadFunc Shared pointer to the ThreadFuncBase implementation to execute
     */
    PriorityThread(uint64_t id, uint64_t priority, std::shared_ptr<ThreadFuncBase> pThreadFunc);

    /**
     * @brief Three-way comparison operator for PriorityThreads.
     *
     * Compares threads based on their priority values, enabling sorting in priority queues.
     *
     * @param lhs Left-hand-side operand
     * @param rhs Right-hand-side operand
     * @return std::strong_ordering Result of comparing lhs.priority_ with rhs.priority_
     */
    friend auto operator<=>(PriorityThread const& lhs, PriorityThread const& rhs)
    {
        return lhs.priority_ <=> rhs.priority_;
    }

    /**
     * @brief Equality comparison operator for PriorityThreads.
     *
     * Two threads are considered equal if they have the same priority.
     *
     * @param lhs Left-hand-side operand
     * @param rhs Right-hand-side operand
     * @return true if both threads have equal priority; false otherwise
     */
    friend bool operator==(PriorityThread const& lhs, PriorityThread const& rhs)
    {
        return lhs.priority_ == rhs.priority_;
    }

    /**
     * @brief Get the unique identifier for this thread.
     *
     * @return uint64_t The unique ID assigned to this thread
     */
    [[nodiscard]] uint64_t id() const
    {
        return id_;
    }

    /**
     * @brief Get the current priority value of this thread.
     *
     * Higher values indicate higher priority in the scheduling queue.
     *
     * @return uint64_t The current priority value
     */
    [[nodiscard]] uint64_t priority() const
    {
        return priority_;
    }

    /**
     * @brief Increment the priority of this thread.
     *
     * This is typically called when a thread has waited a long time in the queue
     * to prevent starvation of lower-priority threads.
     */
    void increase_priority()
    {
        priority_++;
    }

    /**
     * @brief Get the timestamp when this thread was added to the scheduler queue.
     *
     * @return std::chrono::steady_clock::time_point The arrival time of this thread
     */
    [[nodiscard]] std::chrono::steady_clock::time_point arrival_time() const
    {
        return arrival_time_;
    }

    /**
     * @brief Create and start the thread with its wrapped function.
     *
     * @return std::jthread A joinable thread executing the wrapped function
     */
    std::jthread start() const
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
 * @brief Priority-based thread pool scheduler.
 *
 * This class manages execution of threads in a controlled pool with the following features:
 * <ul>
 *  <li>Executes threads according to their priority (higher priority = earlier execution)</li>
 *  <li>Limits the number of concurrently running threads to pool_size</li>
 *  <li>Implements aging/starvation prevention: boosts priority of waiting threads based on arrival time</li>
 *  <li>Thread-safe queue for adding new threads</li>
 * </ul>
 *
 * The scheduler runs a background thread that processes the priority queue and starts threads
 * when slots become available in the pool. It automatically adjusts priorities of waiting threads
 * at configured time intervals to prevent starvation of low-priority threads.
 */
class ThreadScheduler
{
  public:
    /**
     * @brief Construct a ThreadScheduler with specified configuration.
     *
     * @param priority_intervals Vector of time intervals at which to increase priority of waiting threads.
     *                           Defaults to {50ms, 200ms, 500ms, 1000ms}
     * @param pool_size Maximum number of threads to execute concurrently.
     *                  Defaults to 2x the hardware concurrency
     */
    explicit ThreadScheduler(
        std::vector<millis> const& priority_intervals = default_priority_intervals,
        uint64_t pool_size                            = std::jthread::hardware_concurrency() * 2
    );

    /**
     * @brief Destructor. Terminates the scheduler and waits for queued threads to finish.
     */
    ~ThreadScheduler();

    /**
     * @brief Terminate the scheduler and stop processing new threads.
     *
     * Any threads already started will continue running. This function is thread-safe.
     */
    void terminate();

    /**
     * @brief Add a thread to the scheduler queue with specified priority.
     *
     * The thread will be started when a slot becomes available in the thread pool.
     * This method is thread-safe and returns immediately without waiting for the thread to start.
     *
     * @tparam Func_ Type of the callable object (function, lambda, or functor)
     * @tparam Args_ Variadic parameter types for the function arguments
     * @param id Unique identifier for this thread (for tracking/logging purposes)
     * @param priority Initial priority value (higher = executed sooner)
     * @param func The callable to execute in the thread
     * @param args Zero or more arguments to pass to the callable
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
            std::unique_lock lock(mutex_);
            priority_thread_queue_.push(priority_thread);
        }
        // tell everyone that we have an element in the queue
        cv_.notify_one();
    }

  private:
    void         processQueueThread();
    std::jthread processQueue();

    std::priority_queue<PriorityThread> priority_thread_queue_;
    std::vector<millis>                 priority_intervals_;
    uint64_t                            pool_size_;
    std::mutex                          mutex_;
    std::condition_variable             cv_;
    std::jthread                        queue_processor_thread_;
    bool volatile terminate_ = false;
};

/* NOSONAR S5817: Leave this as example usage
using namespace std;
int main()
{
    auto scheduler = ThreadScheduler{default_priority_intervals, std::thread::hardware_concurrency()};

    for(size_t i = 0; i < 20; i++)
        scheduler.addThread(
         4711 + i,
         0 % 5,
         [](int id)
         {
             cout << "threadID=" << std::this_thread::get_id() << ": Hello from " << id << endl;
             this_thread::sleep_for(millis{1500 * (id % 3)});
             cout << "threadID=" << std::this_thread::get_id() << ": finished " << id <<endl;
         },
         i);

    cout << "main-threadID=" << std::this_thread::get_id() << endl;
    this_thread::sleep_for(std::chrono::seconds{5});

    scheduler.terminate();

    return 0;
}
*/

}; // namespace util

#endif // NS_UTIL_THREADUTIL_H_INCLUDED
