#include <iostream>
#include <fstream>
#include <Eigen/Dense>

// // The function we want to fit to our data
// double func(double x, const Eigen::VectorXd &params) {
//     return params[0] * exp(-params[1] * x) + params[2];
// }

Eigen::VectorXd func(const Eigen::VectorXd &x, const Eigen::VectorXd &params) {
    Eigen::VectorXd y(x.size());
    for (int i = 0; i < x.size(); ++i) {
        y[i] = params[0] * exp(-params[1] * x[i]) + params[2];
    }
    return y;
}
// The derivative of the function with respect to the parameters
Eigen::VectorXd df(double x, const Eigen::VectorXd &params) {
    Eigen::VectorXd result(3);
    result[0] = exp(-params[1] * x);
    result[1] = -x * params[0] * exp(-params[1] * x);
    result[2] = 1.0;
    return result;
}

// The residual of the function for a single observation
// double residual(double x, double y, const Eigen::VectorXd &params) {
//     return y - func(x, params);
// }

// The Jacobian of the residuals
Eigen::MatrixXd jacobian(const Eigen::VectorXd &x, const Eigen::VectorXd &y, const Eigen::VectorXd &params) {
    int n = x.size();
    Eigen::MatrixXd result(n, 3);
    for (int i = 0; i < n; ++i) {
        result.row(i) = df(x[i], params);
    }
    return result;
}

// The Gauss-Newton method
Eigen::VectorXd gauss_newton(const Eigen::VectorXd &x, const Eigen::VectorXd &y, Eigen::VectorXd params) {
    int iterations = 10;  // For example
    for (int i = 0; i < iterations; ++i) {
        Eigen::VectorXd r = y - func(x, params);
        Eigen::MatrixXd J = jacobian(x, y, params);
        params += (J.transpose() * J).ldlt().solve(J.transpose() * r);
    }
    return params;
}

int main() {
    // Your x and y data
    std::string lifetime_path = "/mnt/nvme/zt/rocksdb_kv_sep_flush_kv_sep_lifetime_distribution/mlsm_scripts/ycsb_a/lifetime.csv";
    std::ifstream lifetime_file(lifetime_path.c_str());

    std::string line;
    // skip first line
    std::getline(lifetime_file, line);
  uint64_t count = 0;
  std::vector<uint64_t> x;
  std::vector<uint64_t> y;
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
    Eigen::VectorXd x_eigen(x.size());
    Eigen::VectorXd y_eigen(y.size());
  for (int i = 0; i < x.size(); ++i) {
    x_eigen[i] = x[i];
    y_eigen[i] = y[i];
  }


    // Fill x and y with your data

    // Initial guess for the parameters
    Eigen::VectorXd params(3);
    params << 1.0, 1.0, 1.0;

    // Perform the curve fitting
    params = gauss_newton(x_eigen, y_eigen, params);

    std::cout << "Fitted parameters:\n" << params << std::endl;

    return 0;
}

