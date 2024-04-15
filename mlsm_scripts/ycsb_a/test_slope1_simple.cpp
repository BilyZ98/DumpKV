#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>
int main() {
    // Your x and y data
    std::string lifetime_path = "/mnt/nvme/zt/rocksdb_kv_sep_flush_kv_sep_lifetime_distribution/mlsm_scripts/ycsb_a/lifetime.csv";
    std::ifstream lifetime_file(lifetime_path);

    std::string line;
    // skip first line
    std::getline(lifetime_file, line);
  uint64_t count = 0;
  std::vector<uint64_t> x;
  std::vector<uint64_t> y;
  uint64_t max_lifetime = 0;
    while (std::getline(lifetime_file, line)) {
      // std::istringstream iss(line);
      // uint64_t key, value;
      // if (!(iss >> key >> value)) { break; }  // error
      // lifetime_data.emplace_back(key, value);
      uint64_t lifetime = std::stoull(line);
    max_lifetime = std::max(max_lifetime, lifetime);
    x.emplace_back(lifetime);
    y.emplace_back(++count);
      // x.push_back(lifetime);
    // y.push_back(++count);
      
    }
  std::cout << "read finsihed" << std::endl;
  std::sort(x.begin(), x.end());
  std::vector<double> x_double(x.size());
  std::vector<double> y_double(y.size());
  std::cout << "max_lifetime: " << max_lifetime << std::endl;
  for(size_t i = 0; i < x.size(); ++i) {
    x_double[i] = static_cast<double>(x[i]) / static_cast<double>(max_lifetime); 
  }
  for(size_t i = 0; i < y.size(); ++i) {
    y_double[i] = static_cast<double>(y[i]) / static_cast<double>(count);
  }
  size_t slope1_index = 0;
  size_t window_size = 1000;
  std::cout << x[0] << " " << x.back() << std::endl;
  for(size_t i = window_size; i < x.size(); i += window_size) {
    double dy = static_cast<double>(y_double[i]) - static_cast<double>(y_double[i - window_size]);
    double dx = x_double[i] - x_double[i - window_size];
    if(dx > 0.0 && std::abs(dy / dx - 1.0) < 0.1) {
      slope1_index = i;
    }
  }
  std::cout << "slope1_index: " << slope1_index << std::endl;
  std::cout << "lifetime at slope1_index: " << x[slope1_index] << std::endl;


    // Fill x and y with your data

    // Initial guess for the parameters

    return 0;
}

