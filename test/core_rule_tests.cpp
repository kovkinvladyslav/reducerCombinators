static Chapter4ScenarioResult run_link_test() {
  Chapter4ScenarioResult scenario_result = {"Link", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  redex_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 10);
  redex_memory[0].right_port = make_port(PORT_TAG_REFERENCE, 100);
  redex_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 10);
  redex_memory[1].right_port = make_u24_number_port(7);

  ap_uint<32> redex_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 2, &scenario_result);
  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with produced link active pair still queued");
  } else if (scenario_result.statistics.performed_link_count != 2) {
    fail_scenario(&scenario_result, "expected two link interactions");
  } else if (!contains_active_pair_unordered(redex_memory, 4, make_port(PORT_TAG_REFERENCE, 100), make_u24_number_port(7))) {
    fail_scenario(&scenario_result, "missing produced active pair between stored port and second linked port");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_erase_test() {
  Chapter4ScenarioResult scenario_result = {"Erase", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  Port expected_first_auxiliary_port = make_port(PORT_TAG_VARIABLE, 21);
  Port expected_second_auxiliary_port = make_port(PORT_TAG_VARIABLE, 22);
  NodeAddress binary_node_address = allocate_node(node_memory, &allocated_node_count, expected_first_auxiliary_port, expected_second_auxiliary_port, &allocation_successful);
  redex_memory[0].left_port = make_port(PORT_TAG_ERASURE, 0);
  redex_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, binary_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with erase outputs still queued");
  } else if (scenario_result.statistics.performed_erase_count != 1) {
    fail_scenario(&scenario_result, "expected one erase interaction");
  } else if (!contains_active_pair(redex_memory, 4, make_port(PORT_TAG_ERASURE, 0), expected_first_auxiliary_port) ||
             !contains_active_pair(redex_memory, 4, make_port(PORT_TAG_ERASURE, 0), expected_second_auxiliary_port)) {
    fail_scenario(&scenario_result, "missing ERA connections to both auxiliary ports");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_annihilate_test() {
  Chapter4ScenarioResult scenario_result = {"Annihilate", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> left_allocation_successful = 0;
  ap_uint<1> right_allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  Port left_first_auxiliary_port = make_port(PORT_TAG_VARIABLE, 31);
  Port left_second_auxiliary_port = make_port(PORT_TAG_VARIABLE, 32);
  Port right_first_auxiliary_port = make_port(PORT_TAG_VARIABLE, 33);
  Port right_second_auxiliary_port = make_port(PORT_TAG_VARIABLE, 34);
  NodeAddress left_node_address = allocate_node(node_memory, &allocated_node_count, left_first_auxiliary_port, left_second_auxiliary_port, &left_allocation_successful);
  NodeAddress right_node_address = allocate_node(node_memory, &allocated_node_count, right_first_auxiliary_port, right_second_auxiliary_port, &right_allocation_successful);
  redex_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
  redex_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  if (!require_allocation_success(left_allocation_successful) || !require_allocation_success(right_allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with annihilate outputs still queued");
  } else if (scenario_result.statistics.performed_annihilate_count != 1) {
    fail_scenario(&scenario_result, "expected one annihilate interaction");
  } else if (!contains_active_pair(redex_memory, 4, left_first_auxiliary_port, right_first_auxiliary_port) ||
             !contains_active_pair(redex_memory, 4, left_second_auxiliary_port, right_second_auxiliary_port)) {
    fail_scenario(&scenario_result, "missing expected auxiliary-to-auxiliary active pairs");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_commute_test() {
  Chapter4ScenarioResult scenario_result = {"Commute", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> left_allocation_successful = 0;
  ap_uint<1> right_allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 100;
  Port left_first_auxiliary_port = make_port(PORT_TAG_VARIABLE, 1);
  Port left_second_auxiliary_port = make_port(PORT_TAG_VARIABLE, 2);
  Port right_first_auxiliary_port = make_port(PORT_TAG_VARIABLE, 3);
  Port right_second_auxiliary_port = make_port(PORT_TAG_VARIABLE, 4);
  NodeAddress left_node_address = allocate_node(node_memory, &allocated_node_count, left_first_auxiliary_port, left_second_auxiliary_port, &left_allocation_successful);
  NodeAddress right_node_address = allocate_node(node_memory, &allocated_node_count, right_first_auxiliary_port, right_second_auxiliary_port, &right_allocation_successful);
  redex_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
  redex_memory[0].right_port = make_port(PORT_TAG_DUPLICATOR, right_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> node_count_before_reduction = allocated_node_count;
  ap_uint<32> variable_count_before_reduction = allocated_variable_count;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);

  Port fresh_variable_x_port = make_port(PORT_TAG_VARIABLE, variable_count_before_reduction + 0);
  Port fresh_variable_y_port = make_port(PORT_TAG_VARIABLE, variable_count_before_reduction + 1);
  Port fresh_variable_z_port = make_port(PORT_TAG_VARIABLE, variable_count_before_reduction + 2);
  Port fresh_variable_w_port = make_port(PORT_TAG_VARIABLE, variable_count_before_reduction + 3);
  NodeAddress node_ry_address = node_count_before_reduction + 0;
  NodeAddress node_zw_address = node_count_before_reduction + 1;
  NodeAddress node_xz_address = node_count_before_reduction + 2;
  NodeAddress node_yw_address = node_count_before_reduction + 3;

  bool commute_nodes_are_correct =
      get_first_auxiliary_port(node_memory[node_ry_address]) == fresh_variable_x_port &&
      get_second_auxiliary_port(node_memory[node_ry_address]) == fresh_variable_y_port &&
      get_first_auxiliary_port(node_memory[node_zw_address]) == fresh_variable_z_port &&
      get_second_auxiliary_port(node_memory[node_zw_address]) == fresh_variable_w_port &&
      get_first_auxiliary_port(node_memory[node_xz_address]) == fresh_variable_x_port &&
      get_second_auxiliary_port(node_memory[node_xz_address]) == fresh_variable_z_port &&
      get_first_auxiliary_port(node_memory[node_yw_address]) == fresh_variable_y_port &&
      get_second_auxiliary_port(node_memory[node_yw_address]) == fresh_variable_w_port;

  if (!require_allocation_success(left_allocation_successful) || !require_allocation_success(right_allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with commute outputs still queued");
  } else if (scenario_result.statistics.performed_commute_count != 1) {
    fail_scenario(&scenario_result, "expected one commute interaction");
  } else if (allocated_node_count != node_count_before_reduction + 4 || allocated_variable_count != variable_count_before_reduction + 4) {
    fail_scenario(&scenario_result, "commute did not allocate exactly four nodes and four variables");
  } else if (!commute_nodes_are_correct) {
    fail_scenario(&scenario_result, "commute allocated nodes have incorrect cross-wiring");
  } else if (!contains_active_pair(redex_memory, 8, make_port(PORT_TAG_DUPLICATOR, node_ry_address), left_first_auxiliary_port) ||
             !contains_active_pair(redex_memory, 8, make_port(PORT_TAG_DUPLICATOR, node_zw_address), left_second_auxiliary_port) ||
             !contains_active_pair(redex_memory, 8, make_port(PORT_TAG_CONSTRUCTOR, node_xz_address), right_first_auxiliary_port) ||
             !contains_active_pair(redex_memory, 8, make_port(PORT_TAG_CONSTRUCTOR, node_yw_address), right_second_auxiliary_port)) {
    fail_scenario(&scenario_result, "missing expected commute-produced active pairs");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}
