// Copyright © 2021 Thomas Nagler
//
// This file is part of the RcppThread and licensed under the terms of
// the MIT license. For a copy, see the LICENSE.md file in the root directory of
// RcppThread or https://github.com/tnagler/RcppThread/blob/master/LICENSE.md.

#pragma once

#include "RcppThread/ThreadPool.hpp"
#include <algorithm>

namespace RcppThread {

//! computes an index-based for loop in parallel batches.
//! @param begin first index of the loop.
//! @param size the loop runs in the range `[begin, begin + size)`.
//! @param f a function (the 'loop body').
//! @param nThreads the number of threads to use; the default uses the number
//!   of cores in the machine;  if `nThreads = 0`, all work will be done in the
//!   main thread.
//! @param nBatches the number of batches to create; the default (0)
//!   triggers a heuristic to automatically determine the number of batches.
//! @details Consider the following code:
//! ```
//! std::vector<double> x(10);
//! for (size_t i = 0; i < x.size(); i++) {
//!     x[i] = i;
//! }
//! ```
//! The parallel equivalent is
//! ```
//! parallelFor(0, 10, [&] (size_t i) {
//!     x[i] = i;
//! });
//! ```
//! The function sets up a `ThreadPool` object to do the scheduling. If you
//! want to run multiple parallel for loops, consider creating a `ThreadPool`
//! yourself and using `ThreadPool::forEach()`.
//!
//! **Caution**: if the iterations are not independent from another,
//! the tasks need to be synchronized manually (e.g., using mutexes).
template<class F>
inline void parallelFor(ptrdiff_t begin, ptrdiff_t size, F&& f,
                        size_t nThreads = std::thread::hardware_concurrency(),
                        size_t nBatches = 0)
{
    nThreads = std::min((ptrdiff_t)nThreads, size);
    auto workers = LoopWorker::arrange2(begin, begin + size, nThreads);
    std::vector<std::mutex> mRange_(nThreads);
    alignas(64) std::atomic_size_t jobsDone_{0};
    const auto runWorker = [&](size_t id) {
        while (true) {
            ptrdiff_t i, e;
            {
                // std::lock_guard<std::mutex> lk(mRange_[id]);
                i = workers[id].begin++;
                e = workers[id].end;
            }
            std::stringstream msg;
            // msg << "worker " << id << ": "
            //     << "[" << i << ", " << e << ")";
            if (i < e) {
                f(i);
                jobsDone_++;
                // msg << " EXEC\n";
                // Rcout << msg.str();
            } else {
                // msg << " JUMP \n";
                // Rcout << msg.str();
                workers[id].stealRange(workers, mRange_);
                if (workers[id].empty())
                    return;
            }
        }
    };
    std::vector<std::thread> threads;
    for (int k = 0; k != nThreads && jobsDone_ != size; k++)
        threads.push_back(std::thread(runWorker, k));
    for (int k = 0; k != threads.size(); k++)
        threads[k].join();
}

//! computes a range-based for loop in parallel batches.
//! @param items an object allowing for `items.size()` and whose elements
//!   are accessed by the `[]` operator.
//! @param f a function (the 'loop body').
//! @param nThreads the number of threads to use; the default uses the number
//!   of cores in the machine;  if `nThreads = 0`, all work will be done in the
//!   main thread.
//! @param nBatches the number of batches to create; the default (0)
//!   triggers a heuristic to automatically determine the number of batches.
//! @details Consider the following code:
//! ```
//! std::vector<double> x(10, 1.0);
//! for (auto& xx : x) {
//!     xx *= 2;
//! }
//! ```
//! The parallel equivalent is
//! ```
//! parallelFor(x, [&] (double& xx) {
//!     xx *= 2;
//! });
//! ```
//! The function sets up a `ThreadPool` object to do the scheduling. If you
//! want to run multiple parallel for loops, consider creating a `ThreadPool`
//! yourself and using `ThreadPool::forEach()`.
//!
//! **Caution**: if the iterations are not independent from another,
//! the tasks need to be synchronized manually (e.g., using mutexes).
template<class I, class F>
inline void parallelForEach(I& items, F&& f,
    size_t nThreads = std::thread::hardware_concurrency(),
    size_t nBatches = 0)
{
    ThreadPool pool(nThreads);
    pool.parallelForEach(items, f, nBatches);
    pool.join();
}


}
