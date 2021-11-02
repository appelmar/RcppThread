#include "test-ia.hpp"
#include <omp.h>

#include <chrono>
#include <functional>
#include <iostream>

using namespace std;

struct ScopedNanoTimer
{
    chrono::high_resolution_clock::time_point t0;
    function<void(int)> cb;

    ScopedNanoTimer(function<void(int)> callback)
      : t0(chrono::high_resolution_clock::now())
      , cb(callback)
    {}
    ~ScopedNanoTimer(void)
    {
        auto t1 = chrono::high_resolution_clock::now();
        auto nanos =
          chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

        cb(nanos);
    }
};

using namespace Eigen;

VectorXd
kernel(const VectorXd& x)
{
    VectorXd k(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        if (std::abs(x(i)) > 1)
            k(i) = 0;
        else
            k(i) = 0.75 * (1 - std::pow(x(i), 2));
    }
    return k;
}

VectorXd
kde(const VectorXd& x)
{
    double n = x.size();
    double sd = std::sqrt((x.array() - x.mean()).square().sum() / (n - 1));
    double bw = 1.06 * sd * std::pow(n, -0.2);
    VectorXd grid = VectorXd::LinSpaced(500, -1, 1);
    VectorXd fhat(grid.size());
    for (size_t i = 0; i < grid.size(); ++i) {
        fhat(i) = kernel((x.array() - grid(i)) / bw).mean() / bw;
    }

    return fhat;
}

int
main()
{
    for (int i = 0; i < 1; i++) {
        int n = 100;
        int d = 10;
        MatrixXd x = MatrixXd(n, d).setRandom().matrix();

        {
            auto timer = ScopedNanoTimer(
              [](int ns) { cout << "parallelFor: " << ns / 1e6 << endl; });
            for (int j = 0; j < 1; j++) {
                RcppThread::newParallelFor(
                  0, 100000, [&](size_t i) { std::sqrt(x.col(0)(0)); });
            }
        }

        std::this_thread::sleep_for(chrono::seconds(1));

        n = 100;
        d = 10;
        x.setRandom();

        {
            auto timer = ScopedNanoTimer(
              [](int ns) { cout << "OpenMP: " << ns / 1e6 << endl; });

            for (int j = 0; j < 1; j++) {

                omp_set_num_threads(std::thread::hardware_concurrency());
#pragma omp parallel for
                for (size_t i = 0; i < 100000; ++i) {
                    std::sqrt(x.col(0)(0));
                }
            }
        }

        std::this_thread::sleep_for(chrono::seconds(1));

        n = 100;
        d = 10;
        x.setRandom();

        {
            auto timer = ScopedNanoTimer(
              [](int ns) { cout << "parallelFor: " << ns / 1e6 << endl; });
            for (int j = 0; j < 1; j++) {
                RcppThread::newParallelFor(
                  0, 100000, [&](size_t i) { std::sqrt(x.col(0)(0)); });
            }
        }

        // std::thread([] {for (int k = 0; k != 10; k++) std::thread([]
        // {}).detach();}).detach();
    }

    return 0;
}
