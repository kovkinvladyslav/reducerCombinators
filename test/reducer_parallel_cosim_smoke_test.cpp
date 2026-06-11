#include "../src/reducer_parallel_cosim_smoke.hpp"

#include <iostream>

#ifndef LANE_BENCHMARK_PAIR_COUNT
#define LANE_BENCHMARK_PAIR_COUNT 16
#endif

int main() {
  ap_uint<32> active_pair_count = 0;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = 0;
  ap_uint<32> parallel_cycle_count = 0;
  ap_uint<32> multi_commit_cycle_count = 0;
  ap_uint<32> performed_void_count = 0;
  ap_uint<32> maximum_global_queue_count = 0;

  reducer_parallel_cosim_smoke(
      &active_pair_count,
      &performed_steps,
      &status_code,
      &parallel_cycle_count,
      &multi_commit_cycle_count,
      &performed_void_count,
      &maximum_global_queue_count);

  std::cout << "active_pair_count=" << active_pair_count << "\n";
  std::cout << "performed_steps=" << performed_steps << "\n";
  std::cout << "status_code=" << status_code << "\n";
  std::cout << "parallel_cycle_count=" << parallel_cycle_count << "\n";
  std::cout << "multi_commit_cycle_count=" << multi_commit_cycle_count << "\n";
  std::cout << "performed_void_count=" << performed_void_count << "\n";
  std::cout << "maximum_global_queue_count=" << maximum_global_queue_count << "\n";

  if (active_pair_count != 0) {
    return 1;
  }
  if (performed_steps != LANE_BENCHMARK_PAIR_COUNT) {
    return 1;
  }
  if (performed_void_count != LANE_BENCHMARK_PAIR_COUNT) {
    return 1;
  }
  if (status_code != REDUCER_STATUS_SUCCESS) {
    return 1;
  }
  return 0;
}
