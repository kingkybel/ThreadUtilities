/*
 * Repository:  https://github.com/kingkybel/ThreadUtilities
 * File Name:   src/threadutil.cc
 * Description: thread utility functions
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

#include <utility>
// #define DO_TRACE_
#include <dkyb/traceutil.h>

namespace util
{

PriorityThread::PriorityThread(uint64_t id, uint64_t priority, std::shared_ptr<ThreadFuncBase> pThreadFunc)
    : id_(id)
    , priority_(priority)
    , arrival_time_(std::chrono::steady_clock::now())
    , pThreadFunc_(std::move(pThreadFunc))
{
}

ThreadScheduler::ThreadScheduler(std::vector<millis> const& priority_intervals, uint64_t pool_size)
    : priority_intervals_(priority_intervals)
    , pool_size_(pool_size)
{
    queue_processor_thread_ = processQueue();
}

ThreadScheduler::~ThreadScheduler()
{
    queue_processor_thread_.join();
}

void ThreadScheduler::terminate()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        terminate_ = true;
    }
    cv_.notify_all();
}

void ThreadScheduler::processQueueThread()
{
    std::vector<std::thread> thread_pool;

    while (true)
    {
        std::unique_lock<std::mutex> processQueueLock{mutex_};

        // Wait for a thread to be available or the termination signal
        cv_.wait(processQueueLock, [this] { return !priority_thread_queue_.empty() || terminate_; });

        // Check for termination
        if (terminate_)
        {
            break;
        }

        // Fill the thread pool
        while (thread_pool.size() < pool_size_ && !priority_thread_queue_.empty())
        {
            auto priority_thread = priority_thread_queue_.top();

            thread_pool.push_back(priority_thread.start());

            priority_thread_queue_.pop();
        }

        // Check if any threads in the pool have finished
        for (auto it = thread_pool.begin(); it != thread_pool.end();)
        {
            if (it->joinable())
            {
                it->join();
                it = thread_pool.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    // Wait for all remaining threads in the pool to finish
    for (auto& thread: thread_pool)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

std::thread ThreadScheduler::processQueue()
{
    std::thread queueProcessorThread{&ThreadScheduler::processQueueThread, this};
    return queueProcessorThread;
}

}; // namespace util
