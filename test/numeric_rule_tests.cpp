static Chapter4ScenarioResult run_operate_sub_order_test() {
  Chapter4ScenarioResult scenario_result = {"Operate SUB order", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(NUMERIC_TAG_OPERATION_SUBTRACT, 10),
      make_port(PORT_TAG_REFERENCE, 88),
      &allocation_successful);
  redex_memory[0].left_port = make_u24_number_port(3);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);

  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with numeric result still queued");
  } else if (scenario_result.statistics.performed_operate_count != 1) {
    fail_scenario(&scenario_result, "expected one operate interaction");
  } else if (!contains_u24_number_pair(redex_memory, 4, 7)) {
    fail_scenario(&scenario_result, "missing expected U24 result 7 for captured SUB active");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_operate_div_by_zero_test() {
  Chapter4ScenarioResult scenario_result = {"Operate DIV by 0", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(NUMERIC_TAG_OPERATION_DIVIDE, 10),
      make_port(PORT_TAG_REFERENCE, 99),
      &allocation_successful);
  redex_memory[0].left_port = make_u24_number_port(0);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 2, &scenario_result);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
#if ENABLE_NUMERIC_DIVISION
  } else if (scenario_result.status_code != REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO) {
    fail_scenario(&scenario_result, "expected numeric division-by-zero status");
#else
  } else if (scenario_result.status_code != REDUCER_STATUS_INVALID_NUMERIC_OPERATION) {
    fail_scenario(&scenario_result, "expected disabled division to report invalid numeric operation");
#endif
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_operate_remainder_profile_test() {
  Chapter4ScenarioResult scenario_result = {"Operate REM profile", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(NUMERIC_TAG_OPERATION_REMAINDER, 10),
      make_port(PORT_TAG_REFERENCE, 100),
      &allocation_successful);
  redex_memory[0].left_port = make_u24_number_port(3);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
#if ENABLE_NUMERIC_REMAINDER
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with remainder result still queued");
  } else if (!contains_u24_number_pair(redex_memory, 4, 1)) {
    fail_scenario(&scenario_result, "missing expected U24 remainder result");
#else
  } else if (scenario_result.status_code != REDUCER_STATUS_INVALID_NUMERIC_OPERATION) {
    fail_scenario(&scenario_result, "expected disabled remainder to report invalid numeric operation");
#endif
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_operate_u24_result_test(
    const char* test_name,
    NumericTag operation_tag,
    ap_uint<24> captured_operand,
    ap_uint<24> active_operand,
    unsigned int expected_result) {
  Chapter4ScenarioResult scenario_result = {test_name, false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(operation_tag, captured_operand),
      make_port(PORT_TAG_REFERENCE, 201),
      &allocation_successful);
  active_pair_memory[0].left_port = make_u24_number_port(active_operand);
  active_pair_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> active_pair_count = 1;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with numeric result still queued");
  } else if (scenario_result.statistics.performed_operate_count != 1) {
    fail_scenario(&scenario_result, "expected one operate interaction");
  } else if (!contains_u24_number_pair(active_pair_memory, 4, expected_result)) {
    fail_scenario(&scenario_result, "missing expected U24 numeric result");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_operate_i24_result_test(
    const char* test_name,
    NumericTag operation_tag,
    ap_int<24> captured_operand,
    ap_int<24> active_operand,
    int expected_result) {
  Chapter4ScenarioResult scenario_result = {test_name, false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(operation_tag, NumericPayload(captured_operand)),
      make_port(PORT_TAG_REFERENCE, 202),
      &allocation_successful);
  active_pair_memory[0].left_port = make_i24_number_port(active_operand);
  active_pair_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> active_pair_count = 1;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with numeric result still queued");
  } else if (scenario_result.statistics.performed_operate_count != 1) {
    fail_scenario(&scenario_result, "expected one operate interaction");
  } else if (!contains_i24_number_pair(active_pair_memory, 4, expected_result)) {
    fail_scenario(&scenario_result, "missing expected I24 numeric result");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_operate_add_u24_test() {
  return run_operate_u24_result_test("Operate ADD U24", NUMERIC_TAG_OPERATION_ADD, 10, 3, 13);
}

static Chapter4ScenarioResult run_operate_mul_u24_test() {
  return run_operate_u24_result_test("Operate MUL U24", NUMERIC_TAG_OPERATION_MULTIPLY, 10, 3, 30);
}

static Chapter4ScenarioResult run_operate_eq_u24_test() {
  return run_operate_u24_result_test("Operate EQ U24", NUMERIC_TAG_OPERATION_EQUAL, 10, 10, 1);
}

static Chapter4ScenarioResult run_operate_lt_u24_test() {
  return run_operate_u24_result_test("Operate LT U24", NUMERIC_TAG_OPERATION_LESS_THAN, 3, 10, 1);
}

static Chapter4ScenarioResult run_operate_u24_sub_underflow_test() {
  return run_operate_u24_result_test("Operate U24 underflow", NUMERIC_TAG_OPERATION_SUBTRACT, 0, 1, 16777215u);
}

static Chapter4ScenarioResult run_operate_i24_negative_add_test() {
  return run_operate_i24_result_test("Operate I24 negative", NUMERIC_TAG_OPERATION_ADD, -5, 2, -3);
}

static Chapter4ScenarioResult run_operate_delayed_test() {
  Chapter4ScenarioResult scenario_result = {"Operate delayed", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_port(PORT_TAG_CONSTRUCTOR, 40),
      make_port(PORT_TAG_REFERENCE, 60),
      &allocation_successful);
  redex_memory[0].left_port = make_u24_number_port(3);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> node_count_before_reduction = allocated_node_count;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  NodeAddress delayed_operator_node_address = node_count_before_reduction;
  Port delayed_operator_node_port = make_port(PORT_TAG_OPERATOR, delayed_operator_node_address);
  bool delayed_node_is_correct =
      get_first_auxiliary_port(node_memory[delayed_operator_node_address]) == make_u24_number_port(3) &&
      get_second_auxiliary_port(node_memory[delayed_operator_node_address]) == make_port(PORT_TAG_REFERENCE, 60);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with delayed operator active pair still queued");
  } else if (allocated_node_count != node_count_before_reduction + 1) {
    fail_scenario(&scenario_result, "operate delayed did not allocate exactly one node");
  } else if (!delayed_node_is_correct) {
    fail_scenario(&scenario_result, "delayed operator node has incorrect auxiliaries");
  } else if (!contains_active_pair(redex_memory, 4, make_port(PORT_TAG_CONSTRUCTOR, 40), delayed_operator_node_port)) {
    fail_scenario(&scenario_result, "missing expected delayed operate active pair");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_switch_zero_test() {
  Chapter4ScenarioResult scenario_result = {"Switch zero", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress switch_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_port(PORT_TAG_REFERENCE, 120),
      make_port(PORT_TAG_REFERENCE, 121),
      &allocation_successful);
  Port switch_first_auxiliary_port = make_port(PORT_TAG_REFERENCE, 120);
  Port switch_second_auxiliary_port = make_port(PORT_TAG_REFERENCE, 121);
  redex_memory[0].left_port = make_u24_number_port(0);
  redex_memory[0].right_port = make_port(PORT_TAG_SWITCH, switch_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> node_count_before_reduction = allocated_node_count;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  NodeAddress zero_branch_constructor_node_address = node_count_before_reduction;
  Port zero_branch_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, zero_branch_constructor_node_address);
  bool zero_branch_node_is_correct =
      get_first_auxiliary_port(node_memory[zero_branch_constructor_node_address]) == switch_second_auxiliary_port &&
      get_second_auxiliary_port(node_memory[zero_branch_constructor_node_address]) == make_port(PORT_TAG_ERASURE, 0);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with switch output still queued");
  } else if (scenario_result.statistics.performed_switch_count != 1) {
    fail_scenario(&scenario_result, "expected one switch interaction");
  } else if (allocated_node_count != node_count_before_reduction + 1) {
    fail_scenario(&scenario_result, "switch zero did not allocate exactly one node");
  } else if (!zero_branch_node_is_correct) {
    fail_scenario(&scenario_result, "switch zero constructor node has incorrect structure");
  } else if (!contains_active_pair(redex_memory, 4, zero_branch_constructor_node_port, switch_first_auxiliary_port)) {
    fail_scenario(&scenario_result, "missing expected switch zero active pair");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_switch_nonzero_test() {
  Chapter4ScenarioResult scenario_result = {"Switch nonzero", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress switch_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_port(PORT_TAG_REFERENCE, 130),
      make_port(PORT_TAG_REFERENCE, 131),
      &allocation_successful);
  Port switch_first_auxiliary_port = make_port(PORT_TAG_REFERENCE, 130);
  Port switch_second_auxiliary_port = make_port(PORT_TAG_REFERENCE, 131);
  redex_memory[0].left_port = make_u24_number_port(3);
  redex_memory[0].right_port = make_port(PORT_TAG_SWITCH, switch_node_address);
  ap_uint<32> redex_count = 1;
  ap_uint<32> node_count_before_reduction = allocated_node_count;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  NodeAddress inner_constructor_node_address = node_count_before_reduction;
  NodeAddress outer_constructor_node_address = node_count_before_reduction + 1;
  Port inner_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, inner_constructor_node_address);
  Port outer_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, outer_constructor_node_address);
  bool inner_node_is_correct =
      get_first_auxiliary_port(node_memory[inner_constructor_node_address]) == make_u24_number_port(2) &&
      get_second_auxiliary_port(node_memory[inner_constructor_node_address]) == switch_second_auxiliary_port;
  bool outer_node_is_correct =
      get_first_auxiliary_port(node_memory[outer_constructor_node_address]) == make_port(PORT_TAG_ERASURE, 0) &&
      get_second_auxiliary_port(node_memory[outer_constructor_node_address]) == inner_constructor_node_port;
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with switch output still queued");
  } else if (scenario_result.statistics.performed_switch_count != 1) {
    fail_scenario(&scenario_result, "expected one switch interaction");
  } else if (allocated_node_count != node_count_before_reduction + 2) {
    fail_scenario(&scenario_result, "switch nonzero did not allocate exactly two nodes");
  } else if (!inner_node_is_correct || !outer_node_is_correct) {
    fail_scenario(&scenario_result, "switch nonzero constructor structure is incorrect");
  } else if (!contains_active_pair(redex_memory, 4, outer_constructor_node_port, switch_first_auxiliary_port)) {
    fail_scenario(&scenario_result, "missing expected switch nonzero active pair");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_unsupported_f24_test() {
  Chapter4ScenarioResult scenario_result = {"F24 ADD support", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(NUMERIC_TAG_OPERATION_ADD, get_numeric_payload_from_port(make_f24_number_port(1.0f))),
      make_port(PORT_TAG_REFERENCE, 150),
      &allocation_successful);
  redex_memory[0].left_port = make_f24_number_port(2.5f);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
#if ENABLE_NUMERIC_F24
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with F24 result still queued");
  } else if (scenario_result.statistics.performed_operate_count != 1) {
    fail_scenario(&scenario_result, "expected one F24 operate interaction");
  } else if (!contains_f24_number_pair(redex_memory, 4, 3.5f)) {
    fail_scenario(&scenario_result, "missing expected F24 result 3.5");
#else
  } else if (scenario_result.status_code != REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE) {
    fail_scenario(&scenario_result, "expected disabled F24 to report unsupported numeric type");
#endif
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_f24_remainder_profile_test() {
  Chapter4ScenarioResult scenario_result = {"F24 REM profile", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  NodeAddress operator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_numeric_operation_port(NUMERIC_TAG_OPERATION_REMAINDER, get_numeric_payload_from_port(make_f24_number_port(5.5f))),
      make_port(PORT_TAG_REFERENCE, 151),
      &allocation_successful);
  redex_memory[0].left_port = make_f24_number_port(2.0f);
  redex_memory[0].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  ap_uint<32> redex_count = 1;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 1, &scenario_result);
  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
#if ENABLE_NUMERIC_F24 && ENABLE_NUMERIC_F24_REMAINDER
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected max-steps status with F24 remainder result still queued");
  } else if (scenario_result.statistics.performed_operate_count != 1) {
    fail_scenario(&scenario_result, "expected one F24 remainder operate interaction");
  } else if (!contains_f24_number_pair(redex_memory, 4, 1.5f)) {
    fail_scenario(&scenario_result, "missing expected F24 remainder result 1.5");
#elif ENABLE_NUMERIC_F24
  } else if (scenario_result.status_code != REDUCER_STATUS_INVALID_NUMERIC_OPERATION) {
    fail_scenario(&scenario_result, "expected disabled F24 remainder to report invalid numeric operation");
#else
  } else if (scenario_result.status_code != REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE) {
    fail_scenario(&scenario_result, "expected disabled F24 to report unsupported numeric type");
#endif
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}
