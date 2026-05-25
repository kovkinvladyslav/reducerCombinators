static Chapter4ScenarioResult run_mixed_reduction_test() {
  Chapter4ScenarioResult scenario_result = {"Mixed reduction", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair redex_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(redex_memory);

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 20;
  NodeAddress left_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 81), make_port(PORT_TAG_VARIABLE, 82), &allocation_successful);
  NodeAddress right_node_address = allocate_node(node_memory, &allocated_node_count, make_port(PORT_TAG_VARIABLE, 83), make_port(PORT_TAG_VARIABLE, 84), &allocation_successful);
  NodeAddress operator_node_address = allocate_node(node_memory, &allocated_node_count, make_numeric_operation_port(NUMERIC_TAG_OPERATION_ADD, 7), make_port(PORT_TAG_VARIABLE, 170), &allocation_successful);

  redex_memory[0].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
  redex_memory[0].right_port = make_port(PORT_TAG_CONSTRUCTOR, right_node_address);
  redex_memory[1].left_port = make_u24_number_port(5);
  redex_memory[1].right_port = make_port(PORT_TAG_OPERATOR, operator_node_address);
  redex_memory[2].left_port = make_port(PORT_TAG_VARIABLE, 50);
  redex_memory[2].right_port = make_u24_number_port(500);
  redex_memory[3].left_port = make_port(PORT_TAG_VARIABLE, 50);
  redex_memory[3].right_port = make_u24_number_port(8);
  ap_uint<32> redex_count = 4;
  run_reducer_scenario(node_memory, substitution_memory, redex_memory, &redex_count, &allocated_node_count, &allocated_variable_count, 32, &scenario_result);
  scenario_result.passed =
      scenario_result.status_code == REDUCER_STATUS_SUCCESS &&
      scenario_result.statistics.performed_annihilate_count >= 1 &&
      scenario_result.statistics.performed_operate_count >= 1 &&
      scenario_result.statistics.performed_link_count >= 2;
  return scenario_result;
}
