#include "../src/reducer_sum_tree_scaled_cosim.hpp"

#include <iostream>

int main() {
  ap_uint<32> frontier_sum = 0;
  ap_uint<32> root_constructor_address = 0;
  ap_uint<32> active_pair_count = 0;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = 0;
  ap_uint<32> call_count = 0;
  ap_uint<32> operate_count = 0;
  ap_uint<32> max_queue_count = 0;
  ap_uint<32> max_node_count = 0;
  ap_uint<32> max_variable_count = 0;

  reducer_sum_tree_scaled_cosim(
      &frontier_sum,
      &root_constructor_address,
      &active_pair_count,
      &performed_steps,
      &status_code,
      &call_count,
      &operate_count,
      &max_queue_count,
      &max_node_count,
      &max_variable_count);

  std::cout << "frontier_sum=" << frontier_sum << "\n";
  std::cout << "root_constructor_address=" << root_constructor_address << "\n";
  std::cout << "active_pair_count=" << active_pair_count << "\n";
  std::cout << "performed_steps=" << performed_steps << "\n";
  std::cout << "status_code=" << status_code << "\n";
  std::cout << "call_count=" << call_count << "\n";
  std::cout << "operate_count=" << operate_count << "\n";
  std::cout << "max_queue_count=" << max_queue_count << "\n";
  std::cout << "max_node_count=" << max_node_count << "\n";
  std::cout << "max_variable_count=" << max_variable_count << "\n";

  if (frontier_sum != 28) return 1;
  if (root_constructor_address != 4) return 1;
  if (active_pair_count != 0) return 1;
  if (performed_steps != 10) return 1;
  if (status_code != REDUCER_STATUS_SUCCESS) return 1;
  if (call_count != 1) return 1;
  if (operate_count != 4) return 1;
  return 0;
}
