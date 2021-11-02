// Copyright © 2021 Thomas Nagler
//
// This file is part of the RcppThread and licensed under the terms of
// the MIT license. For a copy, see the LICENSE.md file in the root directory of
// RcppThread or https://github.com/tnagler/RcppThread/blob/master/LICENSE.md.

#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <eigen3/Eigen/Dense>
#include <condition_variable>
#include <iostream>
#include "../inst/include/RcppThread/Batch.hpp"

namespace RcppThread {

template<typename F>
struct RcppThreadJob
{
    alignas(64) F func_;

    RcppThreadJob() {}
    RcppThreadJob(F&& f)
        : func_(std::forward<F>(f))
    {}
    decltype(func_()) operator()() { return func_(); }
};

class LoopWorker
{
public:
    LoopWorker() {}
    LoopWorker(ptrdiff_t begin, ptrdiff_t end, size_t id)
        : begin(begin)
    , end(end)
    , id(id)
    {}
    LoopWorker(LoopWorker&& other)
    {
        begin.store(other.begin.load());
        end.store(other.end.load());
        id = other.id;
    }

    LoopWorker(const LoopWorker& other)
    {
        begin.store(other.begin.load());
        end.store(other.end.load());
        id = other.id;
    }

    LoopWorker& operator=(const LoopWorker& other)
    {
        begin.store(other.begin.load());
        end.store(other.end.load());
        id = other.id;
        return *this;
    }

    bool empty() const { return this->size() < 1; }
    ptrdiff_t size() const { return end - begin; }

    void stealRange(std::vector<LoopWorker>& workers) 
    //, std::vector<std::mutex>& mtx)
    {
        size_t k = 0;
        auto nWorkers = workers.size();
        occupied.test_and_set();
        for (size_t k = 1; k < nWorkers; k++) {
            auto& other = workers[(id + k) % nWorkers];
            if (other.occupied.test_and_set())
                continue;
            
            ptrdiff_t size = other.size();
            if (size < 1)
                continue;
            // std::stringstream msg;
            // msg << "split: "
            //     << "[" << other.begin << ", " << other.end << ") ";
            auto shift = (size + 1) / 2;  // shift at least by 1
            auto oldEnd = other.end.load();
            auto newBegin = other.begin.fetch_add(shift);

            end.store(std::min(newBegin + shift, oldEnd));
            begin.store(newBegin);

            occupied.clear();
            other.occupied.clear();
            // msg << "-> "
            //     << "[" << begin << ", " << end
            //     << ") + [" << other.begin << ", " << other.end << ")\n";
            // std::cout << msg.str();
            return;
        }
    }

    static std::vector<LoopWorker> arrange(ptrdiff_t begin,
                                           ptrdiff_t end,
                                           size_t nWorkers)
    {
        auto nTasks = end - begin;
        std::vector<LoopWorker> workers(nWorkers);
        if (nTasks == 0)
            return workers;

        size_t minSize = nTasks / nWorkers;
        ptrdiff_t remSize = nTasks % nWorkers;

        for (size_t i = 0, k = 0; i < nTasks; k++) {
            ptrdiff_t bBegin = begin + i;
            ptrdiff_t bSize = minSize + (remSize-- > 0);
            workers[k] = LoopWorker(bBegin, bBegin + bSize, k);
            i += bSize;
        }

        return workers;
    }

    static std::vector<LoopWorker> arrange2(ptrdiff_t begin,
                                            ptrdiff_t end,
                                            size_t nWorkers)
    {
        auto workers = std::vector<LoopWorker>(nWorkers);
        if (nWorkers == 0)
            return workers;
        workers[0] = LoopWorker(begin, end, 0);
        for (size_t k = 1; k < workers.size(); k++) {
            workers[k] = LoopWorker(end, end, k);
        }

        return workers;
    }

    alignas(64) std::atomic_ptrdiff_t begin{ 0 };
    alignas(64) std::atomic_ptrdiff_t end{ 0 };
    alignas(64) std::atomic_flag occupied{ false };
    alignas(64) size_t id{ 0 };
};


// [[Rcpp::export]]
template<class F>
inline void newParallelFor(ptrdiff_t begin, ptrdiff_t size, F&& f,
                        size_t nThreads = std::thread::hardware_concurrency(),
                        size_t nBatches = 0)
{
    // size = 10;
    nThreads = std::min((ptrdiff_t)nThreads, size);
    auto workers = std::shared_ptr<LoopWorkernew LoopWorker::arrange(begin, begin + size, nThreads);
    // auto batches = createBatches(begin, size, nThreads, 0);
    alignas(64) std::atomic_size_t jobsDone_{0};
    alignas(64) auto const ff = [f] (ptrdiff_t i) { f(i); };
    // std::condition_variable cv;
    // std::mutex mtx;
    // return;
    const auto runWorker = [&](size_t id) {
        // auto e = workers[id].end.load();
        // for (int i = workers[id].begin; i < e; i++)
        //     {f(i);  jobsDone_++;}
        // return;
        while (true) {
            ptrdiff_t i, e;
            {
                // std::lock_guard<std::mutex> lk(mRange_[id]);
                i = workers[id].begin++;
                e = workers[id].end;
            }
            // while (i < e) {
            //     f(i++);    
            //     jobsDone_.fetch_add(1, std::memory_order_relaxed);
            // }
            // return;
            // std::stringstream msg;
            // msg << "worker " << id << ": "
            //     << "[" << i << ", " << e << ")";
            if (i < e) {
                f(i);
                jobsDone_.fetch_add(1, std::memory_order_relaxed);
                // msg << " EXEC\n";
                // Rcout << msg.str();
            } else {
                // std::cout << " JUMP \n";
                workers[id].stealRange(workers);
                if (workers[id].empty())
                    return;
            }
        }
    };
    std::vector<std::thread> threads(nThreads);
    for (int k = 0; k != nThreads; k++) {
         threads[k] = 
        std::thread(runWorker, k);
    }
    for (auto& t : threads) t.join();

    // for (int k = 0; k != nThreads; k++) {
    //     std::thread(runWorker, k).detach();
    // }

    // for (int k = 0; k != nThreads; k++) {
    //     if (threads[k].joinable()) threads[k].join();
    // }
        // static auto timeout = std::chrono::milliseconds(500);

    // std::unique_lock<std::mutex> lk(mtx);
    //    cv.wait(lk, [&] {return jobsDone_ == size; });
    // while (jobsDone_ != size) std::this_thread::yield();
}

}
