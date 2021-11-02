Rcpp::sourceCpp(code = {
'
// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::depends(RcppThread)]]

#include "RcppThread.h"

// [[Rcpp::export]]
void testfor()
{
    std::vector<int> x(2000, 1);
    RcppThread::parallelFor(0, x.size(), [&] (int i) {
        x[i] = 2 * x[i];
    });
    for (int i = 0; i != x.size(); i++) {
        // RcppThread::Rcout << "[" << i << "] " << x[i] << std::endl;
         if (x[i] != 2)
            throw std::runtime_error("mismatch");
    }
}
'
})

testfor()

