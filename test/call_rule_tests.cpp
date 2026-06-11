static BookDefinitionHeader make_test_book_header(
    ap_uint<1> definition_is_safe,
    Port root_port,
    ap_uint<32> redex_count,
    ap_uint<32> node_count,
    ap_uint<32> variable_count,
    ap_uint<32> redex_base_index,
    ap_uint<32> node_base_index) {
  BookDefinitionHeader header;
  header.definition_is_safe = definition_is_safe;
  header.root_port = root_port;
  header.redex_count = redex_count;
  header.node_count = node_count;
  header.variable_count = variable_count;
  header.redex_base_index = redex_base_index;
  header.node_base_index = node_base_index;
  return header;
}

static bool test_book_port_adjustment() {
  ap_uint<1> adjustment_successful = 1;
  Port adjusted_variable_port = adjust_book_port(
      make_port(PORT_TAG_VARIABLE, 2),
      20,
      100,
      4,
      5,
      &adjustment_successful);
  Port adjusted_constructor_port = adjust_book_port(
      make_port(PORT_TAG_CONSTRUCTOR, 3),
      20,
      100,
      4,
      5,
      &adjustment_successful);
  Port adjusted_literal_port = adjust_book_port(
      make_u24_number_port(11),
      20,
      100,
      4,
      5,
      &adjustment_successful);
  return adjustment_successful &&
         adjusted_variable_port == make_port(PORT_TAG_VARIABLE, 102) &&
         adjusted_constructor_port == make_port(PORT_TAG_CONSTRUCTOR, 23) &&
         adjusted_literal_port == make_u24_number_port(11);
}

static Chapter4ScenarioResult run_call_identity_test() {
  Chapter4ScenarioResult scenario_result = {"Call identity", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_VARIABLE, 0), 0, 0, 1, 0, 0);
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(42);

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 10;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected queued remapped identity link after one call step");
  } else if (scenario_result.statistics.performed_call_count != 1) {
    fail_scenario(&scenario_result, "expected one call interaction");
  } else if (allocated_variable_count != 11) {
    fail_scenario(&scenario_result, "identity call did not allocate one fresh variable");
  } else if (!contains_active_pair_unordered(active_pair_memory, 4, make_port(PORT_TAG_VARIABLE, 10), make_u24_number_port(42))) {
    fail_scenario(&scenario_result, "missing adjusted root variable linked to call argument");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_constructor_test() {
  Chapter4ScenarioResult scenario_result = {"Call constructor", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_CONSTRUCTOR, 0), 0, 1, 0, 0, 0);
  book_node_memory[0] = make_packed_node(make_u24_number_port(1), make_u24_number_port(2));
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_root_port();

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 5;
  ap_uint<32> allocated_variable_count = 10;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected constructor root active pair after one call step");
  } else if (scenario_result.statistics.performed_call_count != 1) {
    fail_scenario(&scenario_result, "expected one call interaction");
  } else if (allocated_node_count != 6) {
    fail_scenario(&scenario_result, "constructor call did not allocate one execution node");
  } else if (get_first_auxiliary_port(node_memory[5]) != make_u24_number_port(1) ||
             get_second_auxiliary_port(node_memory[5]) != make_u24_number_port(2)) {
    fail_scenario(&scenario_result, "copied constructor node has incorrect auxiliary ports");
  } else if (!contains_active_pair_unordered(active_pair_memory, 4, make_port(PORT_TAG_CONSTRUCTOR, 5), make_root_port())) {
    fail_scenario(&scenario_result, "missing adjusted constructor root linked to target");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_internal_active_pair_test() {
  Chapter4ScenarioResult scenario_result = {"Call internal pair", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_VARIABLE, 1), 1, 0, 2, 0, 0);
  book_redex_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 0);
  book_redex_memory[0].right_port = make_u24_number_port(5);
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(9);

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 20;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected remapped internal active pair and root pair queued");
  } else if (allocated_variable_count != 22) {
    fail_scenario(&scenario_result, "definition did not allocate two fresh variables");
  } else if (!contains_active_pair_unordered(active_pair_memory, 5, make_port(PORT_TAG_VARIABLE, 20), make_u24_number_port(5)) ||
             !contains_active_pair_unordered(active_pair_memory, 5, make_port(PORT_TAG_VARIABLE, 21), make_u24_number_port(9))) {
    fail_scenario(&scenario_result, "missing adjusted internal active pair or adjusted root pair");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_nested_reference_test() {
  Chapter4ScenarioResult scenario_result = {"Call nested ref", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_REFERENCE, 1), 0, 0, 0, 0, 0);
  book_definition_headers[1] = make_test_book_header(1, make_u24_number_port(77), 0, 0, 0, 0, 0);
  book_definition_count = 2;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_root_port();

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      3,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_SUCCESS) {
    fail_scenario(&scenario_result, "nested reference expansion should finish");
  } else if (scenario_result.statistics.performed_call_count != 2 ||
             scenario_result.statistics.performed_link_count != 1) {
    fail_scenario(&scenario_result, "expected two calls followed by one root link");
  } else if (substitution_memory[ROOT_VARIABLE_INDEX] != make_u24_number_port(77)) {
    fail_scenario(&scenario_result, "root variable did not receive final nested reference value");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_safe_duplication_test() {
  Chapter4ScenarioResult scenario_result = {"Call safe DUP copy", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_u24_number_port(1), 0, 0, 0, 0, 0);
  book_definition_count = 1;

  ap_uint<1> allocation_successful = 0;
  ap_uint<32> allocated_node_count = 0;
  NodeAddress duplicator_node_address = allocate_node(
      node_memory,
      &allocated_node_count,
      make_u24_number_port(1),
      make_u24_number_port(2),
      &allocation_successful);
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_port(PORT_TAG_DUPLICATOR, duplicator_node_address);

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (!require_allocation_success(allocation_successful)) {
    fail_scenario(&scenario_result, "setup allocation failed");
  } else if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED) {
    fail_scenario(&scenario_result, "expected copied reference active pairs still queued");
  } else if (allocated_node_count != 1 || allocated_variable_count != 0) {
    fail_scenario(&scenario_result, "safe DUP copy should not instantiate the definition");
  } else if (!contains_active_pair_unordered(active_pair_memory, 5, make_port(PORT_TAG_REFERENCE, 0), make_u24_number_port(1)) ||
             !contains_active_pair_unordered(active_pair_memory, 5, make_port(PORT_TAG_REFERENCE, 0), make_u24_number_port(2))) {
    fail_scenario(&scenario_result, "missing copied REF active pairs for DUP auxiliaries");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_unknown_reference_test() {
  Chapter4ScenarioResult scenario_result = {"Unknown reference", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);

  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 3);
  active_pair_memory[0].right_port = make_u24_number_port(1);
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
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_UNKNOWN_REFERENCE) {
    fail_scenario(&scenario_result, "unknown REF id should report unknown reference");
  } else if (scenario_result.statistics.unknown_reference_count != 1) {
    fail_scenario(&scenario_result, "unknown reference counter did not increment");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_node_capacity_test() {
  Chapter4ScenarioResult scenario_result = {"Call node capacity", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_CONSTRUCTOR, 0), 0, 1, 0, 0, 0);
  book_node_memory[0] = make_packed_node(make_u24_number_port(1), make_u24_number_port(2));
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_root_port();

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = MAXIMUM_NODE_COUNT;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_NODE_MEMORY_OVERFLOW) {
    fail_scenario(&scenario_result, "call should fail cleanly when node memory is full");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_variable_capacity_test() {
  Chapter4ScenarioResult scenario_result = {"Call variable capacity", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_port(PORT_TAG_VARIABLE, 0), 0, 0, 1, 0, 0);
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(1);

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = MAXIMUM_ALLOCATABLE_VARIABLE_COUNT;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_VARIABLE_MEMORY_OVERFLOW) {
    fail_scenario(&scenario_result, "call should preserve the reserved root variable and report variable overflow");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}

static Chapter4ScenarioResult run_call_queue_capacity_test() {
  Chapter4ScenarioResult scenario_result = {"Call queue capacity", false, 0, 0, 0, 0, {}};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);

  book_definition_headers[0] = make_test_book_header(1, make_u24_number_port(0), 1, 0, 0, 0, 0);
  book_redex_memory[0].left_port = make_u24_number_port(1);
  book_redex_memory[0].right_port = make_u24_number_port(2);
  book_definition_count = 1;
  active_pair_memory[0].left_port = make_port(PORT_TAG_REFERENCE, 0);
  active_pair_memory[0].right_port = make_root_port();

  ap_uint<32> active_pair_count = MAXIMUM_REDEX_QUEUE_LENGTH;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_reducer_scenario_with_book(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      1,
      &scenario_result);

  if (scenario_result.status_code != REDUCER_STATUS_REDEX_QUEUE_OVERFLOW) {
    fail_scenario(&scenario_result, "call should fail when definition redexes plus root link do not fit");
  } else {
    pass_scenario(&scenario_result);
  }
  return scenario_result;
}
