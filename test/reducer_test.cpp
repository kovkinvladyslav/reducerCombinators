#include "reducer_test_common.hpp"

#include "core_rule_tests.cpp"
#include "numeric_rule_tests.cpp"
#include "call_rule_tests.cpp"
#include "boundary_tests.cpp"
#include "mixed_reduction_tests.cpp"
#include "parallel_scheduler_tests.cpp"
#include "program_benchmark_tests.cpp"
#include "reducer_test_reporting.cpp"

int main() {
  bool all_tests_passed = true;
  bool port_encoding_test_passed = test_port_encoding();
  bool node_packing_test_passed = test_node_packing();
  bool numeric_encoding_test_passed = test_numeric_encoding();
  bool book_port_adjustment_test_passed = test_book_port_adjustment();
  bool program_book_capacity_test_passed = test_program_book_builder_capacity();

  std::cout << "Foundational unit checks:\n";
  std::cout << "- Port Encoding Test   : " << (port_encoding_test_passed ? "PASS" : "FAIL") << "\n";
  std::cout << "- Node Packing Test    : " << (node_packing_test_passed ? "PASS" : "FAIL") << "\n";
  std::cout << "- Numeric Encoding Test: " << (numeric_encoding_test_passed ? "PASS" : "FAIL") << "\n";
  std::cout << "- Book Port Adjust Test: " << (book_port_adjustment_test_passed ? "PASS" : "FAIL") << "\n";
  std::cout << "- Program Builder Test : " << (program_book_capacity_test_passed ? "PASS" : "FAIL") << "\n";
  all_tests_passed = all_tests_passed && port_encoding_test_passed && node_packing_test_passed &&
                     numeric_encoding_test_passed && book_port_adjustment_test_passed &&
                     program_book_capacity_test_passed;

  Chapter4ScenarioResult scenario_results[] = {
      run_link_test(),
      run_erase_test(),
      run_annihilate_test(),
      run_commute_test(),
      run_operate_add_u24_test(),
      run_operate_mul_u24_test(),
      run_operate_eq_u24_test(),
      run_operate_lt_u24_test(),
      run_operate_u24_sub_underflow_test(),
      run_operate_i24_negative_add_test(),
      run_operate_sub_order_test(),
      run_operate_div_by_zero_test(),
      run_operate_remainder_profile_test(),
      run_operate_delayed_test(),
      run_switch_zero_test(),
      run_switch_nonzero_test(),
      run_unsupported_f24_test(),
      run_f24_remainder_profile_test(),
      run_call_identity_test(),
      run_call_constructor_test(),
      run_call_internal_active_pair_test(),
      run_call_nested_reference_test(),
      run_call_safe_duplication_test(),
      run_unknown_reference_test(),
      run_call_node_capacity_test(),
      run_call_variable_capacity_test(),
      run_call_queue_capacity_test(),
      run_mixed_reduction_test(),
      run_overflow_test(),
      run_node_overflow_test(),
      run_unsupported_interaction_test(),
      run_empty_input_test(),
      run_maximum_steps_zero_test(),
      run_invalid_variable_index_test(),
  };

  print_scenario_header();
  for (unsigned int scenario_index = 0; scenario_index < array_length(scenario_results); ++scenario_index) {
    print_scenario_result(scenario_results[scenario_index]);
    all_tests_passed = all_tests_passed && scenario_results[scenario_index].passed;
  }
  std::cout << "------------------------------------------------------------------------------------------------------------------------------------------\n";

  Chapter4ScenarioResult parallel_scenario_results[] = {
      run_parallel_independent_void_test(),
      run_parallel_independent_link_test(),
      run_parallel_substitution_bank_metric_test(),
      run_parallel_independent_erase_test(),
      run_parallel_independent_annihilate_test(),
      run_parallel_node_bank_metric_test(),
      run_parallel_allocation_contention_test(),
      run_parallel_substitution_conflict_test(),
      run_parallel_queue_helper_spill_test(),
  };

  print_parallel_scenario_header();
  for (unsigned int scenario_index = 0; scenario_index < array_length(parallel_scenario_results); ++scenario_index) {
    print_parallel_scenario_result(parallel_scenario_results[scenario_index]);
    all_tests_passed = all_tests_passed && parallel_scenario_results[scenario_index].passed;
  }
  std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------\n";

  ProgramBenchmarkResult program_benchmark_results[] = {
      run_hvm_numeric_u24_chain_benchmark(),
      run_hvm_numeric_i24_chain_benchmark(),
      run_hvm_list_constructor_hash_benchmark(),
      run_hvm_sum_rec_scaled_benchmark(),
      run_hvm_sum_tree_scaled_benchmark(),
      run_hvm_stress_scaled_benchmark(),
      run_countdown_loop_benchmark(),
      run_sum_loop_benchmark(),
      run_two_counter_transfer_benchmark(),
      run_commute_fanout_depth_benchmark(),
      run_call_expansion_pressure_benchmark(),
      run_parallel_independent_void_batch_benchmark(),
      run_parallel_mixed_contention_batch_benchmark(),
  };

  print_program_benchmark_header();
  for (unsigned int benchmark_index = 0; benchmark_index < array_length(program_benchmark_results); ++benchmark_index) {
    print_program_benchmark_result(program_benchmark_results[benchmark_index]);
    all_tests_passed = all_tests_passed && program_benchmark_results[benchmark_index].passed;
  }
  std::cout << "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n";

  if (!all_tests_passed) {
    std::cout << "\nFailed scenarios:\n";
    if (!port_encoding_test_passed) std::cout << "- Port Encoding Test: foundational unit check failed\n";
    if (!node_packing_test_passed) std::cout << "- Node Packing Test: foundational unit check failed\n";
    if (!numeric_encoding_test_passed) std::cout << "- Numeric Encoding Test: foundational unit check failed\n";
    if (!book_port_adjustment_test_passed) std::cout << "- Book Port Adjust Test: foundational unit check failed\n";
    if (!program_book_capacity_test_passed) std::cout << "- Program Builder Test: foundational unit check failed\n";
    print_failed_scenarios(scenario_results);
    print_failed_scenarios(parallel_scenario_results);
    print_failed_program_benchmarks(program_benchmark_results);
  }
  std::cout << "Overall Result: " << (all_tests_passed ? "PASS" : "FAIL") << std::endl;
  return all_tests_passed ? 0 : 1;
}
