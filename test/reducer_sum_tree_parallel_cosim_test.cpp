#include "../src/reducer_sum_tree_parallel_cosim.hpp"

#include <iostream>

#ifndef SUM_TREE_TERM_COUNT
#define SUM_TREE_TERM_COUNT 16
#endif

#ifndef SUM_TREE_EXPECTED_RESULT
#define SUM_TREE_EXPECTED_RESULT ((SUM_TREE_TERM_COUNT * (SUM_TREE_TERM_COUNT - 1)) / 2)
#endif

#ifndef SUM_TREE_EXPECTED_STEPS
#define SUM_TREE_EXPECTED_STEPS (SUM_TREE_TERM_COUNT / 2)
#endif

#ifndef SUM_TREE_EXPECTED_OPERATE_COUNT
#define SUM_TREE_EXPECTED_OPERATE_COUNT (SUM_TREE_TERM_COUNT / 2)
#endif

int main() {
  ap_uint<32> result_value = 0;
  ap_uint<32> active_pair_count = 0;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = 0;
  ap_uint<32> operate_count = 0;
  ap_uint<32> parallel_cycle_count = 0;
  ap_uint<32> multi_commit_cycle_count = 0;
  ap_uint<32> maximum_global_queue_count = 0;
  ap_uint<32> max_node_count = 0;
  ap_uint<32> max_variable_count = 0;

  reducer_sum_tree_parallel_cosim(
      &result_value,
      &active_pair_count,
      &performed_steps,
      &status_code,
      &operate_count,
      &parallel_cycle_count,
      &multi_commit_cycle_count,
      &maximum_global_queue_count,
      &max_node_count,
      &max_variable_count);

  std::cout << "result_value=" << result_value << "\n";
  std::cout << "active_pair_count=" << active_pair_count << "\n";
  std::cout << "performed_steps=" << performed_steps << "\n";
  std::cout << "status_code=" << status_code << "\n";
  std::cout << "operate_count=" << operate_count << "\n";
  std::cout << "parallel_cycle_count=" << parallel_cycle_count << "\n";
  std::cout << "multi_commit_cycle_count=" << multi_commit_cycle_count << "\n";
  std::cout << "maximum_global_queue_count=" << maximum_global_queue_count << "\n";
  std::cout << "max_node_count=" << max_node_count << "\n";
  std::cout << "max_variable_count=" << max_variable_count << "\n";

  if (result_value != SUM_TREE_EXPECTED_RESULT) return 1;
  if (active_pair_count != 0) return 1;
  if (performed_steps != SUM_TREE_EXPECTED_STEPS) return 1;
  if (status_code != REDUCER_STATUS_SUCCESS) return 1;
  if (operate_count != SUM_TREE_EXPECTED_OPERATE_COUNT) return 1;
  return 0;
}
