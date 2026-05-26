static Chapter4ScenarioResult run_parallel_independent_void_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel void correctness", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(0);
  active_pair_memory[1].left_port = make_port(PORT_TAG_ERASURE, 7);
  active_pair_memory[1].right_port = make_u24_number_port(1);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_SUCCESS &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_void_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1 &&
      scenario_result.statistics.lane_committed_step_count[0] == 1 &&
      scenario_result.statistics.lane_committed_step_count[1] == 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_allocation_contention_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel allocation metric", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 100;
  NodeAddress first_left_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 1), make_port(PORT_TAG_VARIABLE, 2), &allocation_successful);
  NodeAddress first_right_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 3), make_port(PORT_TAG_VARIABLE, 4), &allocation_successful);
  NodeAddress second_left_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 11), make_port(PORT_TAG_VARIABLE, 12), &allocation_successful);
  NodeAddress second_right_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 13), make_port(PORT_TAG_VARIABLE, 14), &allocation_successful);

  active_pair_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, first_left_node_address);
  active_pair_memory[0].right_port = make_port(PORT_TAG_DUPLICATOR, first_right_node_address);
  active_pair_memory[1].left_port = make_port(PORT_TAG_CONSTRUCTOR, second_left_node_address);
  active_pair_memory[1].right_port = make_port(PORT_TAG_DUPLICATOR, second_right_node_address);

  ap_uint<32> active_pair_count = 2;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_MAXIMUM_STEPS_REACHED &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_commute_count == 1 &&
      scenario_result.statistics.allocation_contention_count >= 1 &&
      scenario_result.statistics.lane_stall_count[1] >= 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_independent_link_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel link dual issue", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 21);
  active_pair_memory[0].right_port = make_port(PORT_TAG_REFERENCE, 210);
  active_pair_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 22);
  active_pair_memory[1].right_port = make_port(PORT_TAG_REFERENCE, 220);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_SUCCESS &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_link_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1 &&
      substitution_memory[21] == make_port(PORT_TAG_REFERENCE, 210) &&
      substitution_memory[22] == make_port(PORT_TAG_REFERENCE, 220);
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_substitution_bank_metric_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel subst bank metric", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 21);
  active_pair_memory[0].right_port = make_port(PORT_TAG_REFERENCE, 210);
  active_pair_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 25);
  active_pair_memory[1].right_port = make_port(PORT_TAG_REFERENCE, 250);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_SUCCESS &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_link_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1 &&
      scenario_result.statistics.substitution_conflict_count == 0 &&
      scenario_result.statistics.substitution_bank_conflict_count >= 1 &&
      substitution_memory[21] == make_port(PORT_TAG_REFERENCE, 210) &&
      substitution_memory[25] == make_port(PORT_TAG_REFERENCE, 250);
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_independent_erase_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel erase dual issue", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  NodeAddress first_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_port(PORT_TAG_VARIABLE, 31),
      make_port(PORT_TAG_VARIABLE, 32),
      &allocation_successful);
  NodeAddress second_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_port(PORT_TAG_VARIABLE, 41),
      make_port(PORT_TAG_VARIABLE, 42),
      &allocation_successful);

  active_pair_memory[0].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, first_node_address);
  active_pair_memory[1].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[1].right_port = make_port(PORT_TAG_CONSTRUCTOR, second_node_address);

  ap_uint<32> active_pair_count = 2;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      require_allocation_success(allocation_successful) &&
      scenario_result.status_code == REDUCER_STATUS_MAXIMUM_STEPS_REACHED &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_erase_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_independent_annihilate_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel anni dual issue", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 64;
  NodeAddress left_a = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 51), make_port(PORT_TAG_VARIABLE, 52), &allocation_successful);
  NodeAddress right_a = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 53), make_port(PORT_TAG_VARIABLE, 54), &allocation_successful);
  NodeAddress left_b = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 61), make_port(PORT_TAG_VARIABLE, 62), &allocation_successful);
  NodeAddress right_b = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 63), make_port(PORT_TAG_VARIABLE, 64), &allocation_successful);

  active_pair_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_a);
  active_pair_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_a);
  active_pair_memory[1].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_b);
  active_pair_memory[1].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_b);

  ap_uint<32> active_pair_count = 2;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      require_allocation_success(allocation_successful) &&
      scenario_result.status_code == REDUCER_STATUS_MAXIMUM_STEPS_REACHED &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_annihilate_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_node_bank_metric_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel node bank metric", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 64;
  NodeAddress left_a = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 51), make_port(PORT_TAG_VARIABLE, 52), &allocation_successful);
  NodeAddress right_a = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 53), make_port(PORT_TAG_VARIABLE, 54), &allocation_successful);
  NodeAddress left_b = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 61), make_port(PORT_TAG_VARIABLE, 62), &allocation_successful);
  NodeAddress right_b = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 63), make_port(PORT_TAG_VARIABLE, 64), &allocation_successful);

  active_pair_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_a);
  active_pair_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_a);
  active_pair_memory[1].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_b);
  active_pair_memory[1].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_b);

  ap_uint<32> active_pair_count = 2;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      require_allocation_success(allocation_successful) &&
      scenario_result.status_code == REDUCER_STATUS_MAXIMUM_STEPS_REACHED &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_annihilate_count == 2 &&
      scenario_result.statistics.dual_commit_cycle_count >= 1 &&
      scenario_result.statistics.node_access_conflict_count == 0 &&
      scenario_result.statistics.node_bank_conflict_count >= 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_substitution_conflict_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel substitution metric", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 9);
  active_pair_memory[0].right_port = make_port(PORT_TAG_REFERENCE, 90);
  active_pair_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 9);
  active_pair_memory[1].right_port = make_port(PORT_TAG_REFERENCE, 91);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  run_parallel_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &scenario_result);

  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_MAXIMUM_STEPS_REACHED &&
      scenario_result.performed_steps == 2 &&
      scenario_result.statistics.performed_link_count == 2 &&
      scenario_result.statistics.substitution_conflict_count >= 1 &&
      scenario_result.statistics.lane_stall_count[1] >= 1;
  return scenario_result;
}

static Chapter4ScenarioResult run_parallel_queue_helper_spill_test() {
  Chapter4ScenarioResult scenario_result = {"Parallel spill metric", false, 0, 0, 0, 0, {}};
  ReducerLaneState reducer_lane_state;
  RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_redex_memory(global_active_pair_memory);
  initialize_reducer_lane_state(&reducer_lane_state);

  RedexPair filler_active_pair;
  filler_active_pair.left_port = make_port(PORT_TAG_ERASURE, 0);
  filler_active_pair.right_port = make_u24_number_port(0);
  for (int local_index = 0; local_index < LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH; ++local_index) {
    active_pair_queue_push_local(&reducer_lane_state, filler_active_pair);
  }

  ap_uint<32> global_queue_tail_index = 0;
  ap_uint<32> global_queue_count_value = 0;
  ap_uint<1> local_push_success = active_pair_queue_push_local(&reducer_lane_state, filler_active_pair);
  ap_uint<1> global_push_success = active_pair_queue_push_global(
      global_active_pair_memory,
      &global_queue_tail_index,
      &global_queue_count_value,
      filler_active_pair);

  scenario_result.performed_steps = 0;
  scenario_result.final_redex_count = global_queue_count_value;
  scenario_result.final_allocated_node_count = 0;
  scenario_result.status_code = REDUCER_STATUS_SUCCESS;
  scenario_result.passed =
      local_push_success == 0 &&
      global_push_success == 1 &&
      global_queue_count_value == 1;
  return scenario_result;
}
