#include <vector>
#include <iostream>
#include <fstream>


#include <stdio.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_roots.h>


//
//  PolyfitEigen.hpp
//  Polyfit
//
//  Created by Patrick Löber on 23.11.18.
//  Copyright © 2018 Patrick Loeber. All rights reserved.
//
//  Use the Eigen library for fitting: http://eigen.tuxfamily.org
//  See https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html for different methods

#include "Eigen/Dense"

template<typename T>
std::vector<T> polyfit_Eigen(const std::vector<T> &xValues, const std::vector<T> &yValues, const int degree, const std::vector<T>& weights = std::vector<T>(), bool useJacobi = false)
{
    using namespace Eigen;
    
    bool useWeights = weights.size() > 0 && weights.size() == xValues.size();
    
    int numCoefficients = degree + 1;
    size_t nCount = xValues.size();
    
    MatrixXf X(nCount, numCoefficients);
    MatrixXf Y(nCount, 1);
    
    // fill Y matrix
    for (size_t i = 0; i < nCount; i++)
    {
        if (useWeights)
            Y(i, 0) = yValues[i] * weights[i];
        else
            Y(i, 0) = yValues[i];
    }
    
    // fill X matrix (Vandermonde matrix)
    for (size_t nRow = 0; nRow < nCount; nRow++)
    {
        T nVal = 1.0f;
        for (int nCol = 0; nCol < numCoefficients; nCol++)
        {
            if (useWeights)
                X(nRow, nCol) = nVal * weights[nRow];
            else
                X(nRow, nCol) = nVal;
            nVal *= xValues[nRow];
        }
    }
    
    VectorXf coefficients;
    if (useJacobi)
        coefficients = X.jacobiSvd(ComputeThinU | ComputeThinV).solve(Y);
    else
        coefficients = X.colPivHouseholderQr().solve(Y);
    
    return std::vector<T>(coefficients.data(), coefficients.data() + numCoefficients);
}

std::vector<double> polyder(const std::vector<double>& coeffs) {
    std::vector<double> result;
    int n = coeffs.size();
    for (int i = 1; i < n; ++i) {
        result.push_back(i * coeffs[i]);
    }
    return result;
}



double polyval(const std::vector<double>& p, double x) {
    double result = 0.0;
    double xi = 1.0;
    for (auto it = p.rbegin(); it != p.rend(); ++it) {
        result += (*it) * xi;
        xi *= x;
    }
    return result;
}

std::vector<double> derivative ;
double f(double x, void *params) {
    // Define your function here
    return polyval(derivative, x) - 1;
}
int main() {

  std::vector<std::string> files = {
  "/mnt/nvme/xq/git_result/rocksdb_kv_sep/mlsm_scripts/ycsb_a_fast/workloada_4096kb_100GB_0.9_zipfian.log_run.formated/lifetime.csv",
  "/mnt/nvme/xq/git_result/rocksdb_kv_sep/mlsm_scripts/ycsb_a_fast/workloada-load-0.99-10000000-100000000.log_run.formated/lifetime.csv",
  "/mnt/nvme/xq/git_result/rocksdb_kv_sep/mlsm_scripts/ycsb_a_fast/workloada-load-10000000-100000000.log_run.formated/lifetime.csv"
  };
    for(auto lifetime_path: files){
    std::cout << lifetime_path << std::endl;
    // std::string lifetime_path = "/mnt/nvme/zt/rocksdb_kv_sep_flush_kv_sep_lifetime_distribution/mlsm_scripts/ycsb_a/lifetime.csv";
    std::ifstream lifetime_file(lifetime_path.c_str());

    std::string line;
    // skip first line
    std::getline(lifetime_file, line);
  uint64_t count = 0;
  std::vector<double> x;
  std::vector<double> y;
    while (std::getline(lifetime_file, line)) {
      // std::istringstream iss(line);
      // uint64_t key, value;
      // if (!(iss >> key >> value)) { break; }  // error
      // lifetime_data.emplace_back(key, value);
      uint64_t lifetime = std::stoull(line);
    x.emplace_back(lifetime);
    y.emplace_back(++count);
      // x.push_back(lifetime);
    // y.push_back(++count);
      
    }
    std::sort(x.begin(), x.end());

    int status;
    int iter = 0, max_iter = 100;
    const gsl_root_fsolver_type *T;
    gsl_root_fsolver *s;
    double r = 0, r_expected = M_PI;
    double x_lo = 0.0, x_hi = 20000000.0;
    gsl_function F;

    F.function = &f;
    F.params = 0;

    auto coeffs = polyfit_Eigen(x, y, 3);
    printf("Coefficients: ");
    for(auto c: coeffs){
      std::cout << c << std::endl;
    }

    derivative = polyder(coeffs);
    printf("Derivative: ");
    for(auto c: derivative){
      std::cout << c << std::endl;
    }
    T = gsl_root_fsolver_brent;
    s = gsl_root_fsolver_alloc(T);
    gsl_set_error_handler_off();
    gsl_root_fsolver_set(s, &F, x_lo, x_hi);

    printf ("using %s method\n", gsl_root_fsolver_name(s));
    printf ("%5s [%9s, %9s] %9s %10s %9s\n", "iter", "lower", "upper", "root", "err", "err(est)");

    do {
        iter++;
        status = gsl_root_fsolver_iterate(s);
        r = gsl_root_fsolver_root(s);
        x_lo = gsl_root_fsolver_x_lower(s);
        x_hi = gsl_root_fsolver_x_upper(s);
        status = gsl_root_test_interval(x_lo, x_hi, 0, 0.001);

        if (status == GSL_SUCCESS)
            printf ("Converged:\n");

        printf ("%5d [%.7f, %.7f] %.7f %+.7f %.7f\n", iter, x_lo, x_hi, r, r - r_expected, x_hi - x_lo);
    } while (status == GSL_CONTINUE && iter < max_iter);

    gsl_root_fsolver_free(s);
  }

    return 0;
}
