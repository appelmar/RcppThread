Rcpp::sourceCpp(code = {
'
// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::depends(RcppThread)]]

#include "RcppThread.h"

// [[Rcpp::export]]
void testfor()
{
    std::vector<int> x(1000000);
    RcppThread::parallelFor(0, 200, [&] (int i) {
        x[i] = i;
    }, 12, 12);
}
'
})

testfor()

