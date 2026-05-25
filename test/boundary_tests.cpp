static Chapter4ScenarioResult run_overflow_test() {
  Chapter4ScenarioResult scenario_result = {"Queue overflow", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress left_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 61), make_port(PORT_TAG_VARIABLE, 62), &allocation_successful);
  NodeAddress right_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 63), make_port(PORT_TAG_VARIABLE, 64), &allocation_successful);
  redex_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
  redex_memory[0].right_port = make_port(PORT_TAG_DUPLICATOR, right_node_address);
  ap_uint<32> redex_count = MAXIMUM_REDEX_COUNT - 2;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  scenario_result.passed = scenario_result.status_code == REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
  return scenario_result;
}

static Chapter4ScenarioResult run_node_overflow_test() {
  Chapter4ScenarioResult scenario_result = {"Node overflow", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress left_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 71), make_port(PORT_TAG_VARIABLE, 72), &allocation_successful);
  NodeAddress right_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 73), make_port(PORT_TAG_VARIABLE, 74), &allocation_successful);
  redex_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
  redex_memory[0].right_port = make_port(PORT_TAG_DUPLICATOR, right_node_address);
  ap_uint<32> redex_count = 1;
  allocated_node_count = MAXIMUM_NODE_COUNT - 3;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 2, &scenario_result);
  scenario_result.passed = scenario_result.status_code == REDUCER_STATUS_NODE_MEMORY_OVERFLOW;
  return scenario_result;
}

static Chapter4ScenarioResult run_unsupported_interaction_test() {
  Chapter4ScenarioResult scenario_result = {"Unsupported", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<32> redex_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  redex_memory[0].left_port = make_u24_number_port(7);
  redex_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, 99);
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 2, &scenario_result);
  scenario_result.passed = scenario_result.status_code == REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  return scenario_result;
}

static Chapter4ScenarioResult run_empty_input_test() {
  Chapter4ScenarioResult scenario_result = {"Empty input", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<32> active_pair_count = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      8,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_SUCCESS) {
    fail_scenario(&scenario_result, "empty input should succeed");
  } else if (scenario_result.performed_steps != 0 || scenario_result.final_redex_count != 0) {
    fail_scenario(&scenario_result, "empty input should perform no steps and leave no active pairs");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_maximum_steps_zero_test() {
  Chapter4ScenarioResult scenario_result = {"Maximum steps zero", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(0);
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      0,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "non-empty input with maximum_steps=0 should report max steps reached");
  } else if (scenario_result.performed_steps != 0 || scenario_result.final_redex_count != 1) {
    fail_scenario(&scenario_result, "maximum_steps=0 should not consume the active pair");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_invalid_variable_index_test() {
  Chapter4ScenarioResult scenario_result = {"Invalid variable index", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, MAXIMUM_VARIABLE_COUNT + 1);
  active_pair_memory[0].right_port = make_u24_number_port(4);
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      4,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_UNSUPPORTED_INTERACTION) {
    fail_scenario(&scenario_result, "invalid variable index should fail with a clean unsupported status");
  } else if (scenario_result.performed_steps != 0) {
    fail_scenario(&scenario_result, "invalid variable index should not count a committed step");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}
