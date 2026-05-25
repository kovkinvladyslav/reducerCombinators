static void print_parallel_scenario_header() {
  std::cout << "\nParallel Active-Pair Scheduling Summary\n";
  std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------\n";
  std::cout << std::left << std::setw(30) << "Test Name"
            << std::setw(6) << "Pass"
            << std::setw(7) << "Steps"
            << std::setw(7) << "Cycle"
            << std::setw(7) << "Dual"
            << std::setw(8) << "Refill"
            << std::setw(8) << "Spill"
            << std::setw(8) << "AllocC"
            << std::setw(8) << "SubstC"
            << std::setw(8) << "NodeC"
            << std::setw(8) << "L0Step"
            << std::setw(8) << "L1Step"
            << std::setw(8) << "L1Stall"
            << std::setw(8) << "Status"
            << "\n";
  std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------\n";
}

static void print_parallel_scenario_result(const Chapter4ScenarioResult& scenario_result) {
  std::cout << std::left << std::setw(30) << scenario_result.test_name
            << std::setw(6) << (scenario_result.passed ? "yes" : "no")
            << std::setw(7) << (unsigned)scenario_result.performed_steps
            << std::setw(7) << (unsigned)scenario_result.statistics.parallel_cycle_count
            << std::setw(7) << (unsigned)scenario_result.statistics.dual_commit_cycle_count
            << std::setw(8) << (unsigned)scenario_result.statistics.global_refill_count
            << std::setw(8) << (unsigned)scenario_result.statistics.local_queue_spill_count
            << std::setw(8) << (unsigned)scenario_result.statistics.allocation_contention_count
            << std::setw(8) << (unsigned)scenario_result.statistics.substitution_conflict_count
            << std::setw(8) << (unsigned)scenario_result.statistics.node_access_conflict_count
            << std::setw(8) << (unsigned)scenario_result.statistics.lane_committed_step_count[0]
            << std::setw(8) << (REDUCER_LANE_COUNT > 1 ? (unsigned)scenario_result.statistics.lane_committed_step_count[1] : 0)
            << std::setw(8) << (REDUCER_LANE_COUNT > 1 ? (unsigned)scenario_result.statistics.lane_stall_count[1] : 0)
            << std::setw(8) << (unsigned)scenario_result.status_code
            << "\n";
}

static void print_scenario_header() {
  std::cout << "\nChapter 4 Scenario Measurement Summary\n";
  std::cout << "------------------------------------------------------------------------------------------------------------------------------------------\n";
  std::cout << std::left << std::setw(28) << "Test Name"
            << std::setw(6) << "Pass"
            << std::setw(7) << "Steps"
            << std::setw(6) << "Link"
            << std::setw(6) << "Void"
            << std::setw(6) << "Erase"
            << std::setw(6) << "Anni"
            << std::setw(6) << "Comm"
            << std::setw(7) << "Oper"
            << std::setw(7) << "Swit"
            << std::setw(7) << "Call"
            << std::setw(7) << "NumErr"
            << std::setw(7) << "UnkRef"
            << std::setw(8) << "Div0"
            << std::setw(8) << "Status"
            << std::setw(8) << "Redex"
            << std::setw(8) << "Nodes"
            << "\n";
  std::cout << "------------------------------------------------------------------------------------------------------------------------------------------\n";
}

static void print_scenario_result(const Chapter4ScenarioResult& scenario_result) {
  std::cout << std::left << std::setw(28) << scenario_result.test_name
            << std::setw(6) << (scenario_result.passed ? "yes" : "no")
            << std::setw(7) << (unsigned)scenario_result.performed_steps
            << std::setw(6) << (unsigned)scenario_result.statistics.performed_link_count
            << std::setw(6) << (unsigned)scenario_result.statistics.performed_void_count
            << std::setw(6) << (unsigned)scenario_result.statistics.performed_erase_count
            << std::setw(6) << (unsigned)scenario_result.statistics.performed_annihilate_count
            << std::setw(6) << (unsigned)scenario_result.statistics.performed_commute_count
            << std::setw(7) << (unsigned)scenario_result.statistics.performed_operate_count
            << std::setw(7) << (unsigned)scenario_result.statistics.performed_switch_count
            << std::setw(7) << (unsigned)scenario_result.statistics.performed_call_count
            << std::setw(7) << (unsigned)scenario_result.statistics.invalid_numeric_operation_count
            << std::setw(7) << (unsigned)scenario_result.statistics.unknown_reference_count
            << std::setw(8) << (unsigned)scenario_result.statistics.numeric_division_by_zero_count
            << std::setw(8) << (unsigned)scenario_result.status_code
            << std::setw(8) << (unsigned)scenario_result.final_redex_count
            << std::setw(8) << (unsigned)scenario_result.final_allocated_node_count
            << "\n";
}

template <unsigned N>
static void print_failed_scenarios(const Chapter4ScenarioResult (&scenario_results)[N]) {
  for (unsigned int scenario_index = 0; scenario_index < N; ++scenario_index) {
    if (!scenario_results[scenario_index].passed) {
      const char* failure_reason = scenario_results[scenario_index].failure_reason;
      if (failure_reason == 0 || failure_reason[0] == '\0') {
        failure_reason = "no detailed failure reason recorded";
      }
      std::cout << "- " << scenario_results[scenario_index].test_name
                << ": " << failure_reason << "\n";
    }
  }
}

static void print_program_benchmark_header() {
  std::cout << "\nProgram Integration Benchmark Summary\n";
  std::cout << "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
  std::cout << std::left << std::setw(30) << "Program"
            << std::setw(6) << "Pass"
            << std::setw(7) << "Input"
            << std::setw(12) << "Expect"
            << std::setw(12) << "Output"
            << std::setw(7) << "Steps"
            << std::setw(6) << "Call"
            << std::setw(6) << "Oper"
            << std::setw(6) << "Swit"
            << std::setw(6) << "Comm"
            << std::setw(8) << "QMax"
            << std::setw(8) << "NMax"
            << std::setw(8) << "VMax"
            << std::setw(8) << "Status"
            << "\n";
  std::cout << "--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
}

static void print_program_benchmark_result(const ProgramBenchmarkResult& benchmark_result) {
  std::cout << std::left << std::setw(30) << benchmark_result.program_name
            << std::setw(6) << (benchmark_result.passed ? "yes" : "no")
            << std::setw(7) << benchmark_result.input_size
            << std::setw(12) << benchmark_result.expected_output
            << std::setw(12) << benchmark_result.observed_output
            << std::setw(7) << (unsigned)benchmark_result.performed_steps
            << std::setw(6) << (unsigned)benchmark_result.statistics.performed_call_count
            << std::setw(6) << (unsigned)benchmark_result.statistics.performed_operate_count
            << std::setw(6) << (unsigned)benchmark_result.statistics.performed_switch_count
            << std::setw(6) << (unsigned)benchmark_result.statistics.performed_commute_count
            << std::setw(8) << (unsigned)benchmark_result.statistics.maximum_observed_redex_queue_count
            << std::setw(8) << (unsigned)benchmark_result.statistics.maximum_observed_allocated_node_count
            << std::setw(8) << (unsigned)benchmark_result.statistics.maximum_observed_allocated_variable_count
            << std::setw(8) << (unsigned)benchmark_result.status_code
            << "\n";
}

template <unsigned N>
static void print_failed_program_benchmarks(const ProgramBenchmarkResult (&benchmark_results)[N]) {
  for (unsigned int benchmark_index = 0; benchmark_index < N; ++benchmark_index) {
    if (!benchmark_results[benchmark_index].passed) {
      const char* failure_reason = benchmark_results[benchmark_index].failure_reason;
      if (failure_reason == 0 || failure_reason[0] == '\0') {
        failure_reason = "no detailed failure reason recorded";
      }
      std::cout << "- " << benchmark_results[benchmark_index].program_name
                << ": " << failure_reason << "\n";
    }
  }
}
