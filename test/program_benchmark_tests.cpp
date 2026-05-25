#include "program_book_builder.hpp"

static void pass_program_benchmark(ProgramBenchmarkResult* benchmark_result) {
  benchmark_result->passed = true;
  benchmark_result->failure_reason = "";
}

static void fail_program_benchmark(
    ProgramBenchmarkResult* benchmark_result,
    const char* failure_reason) {
  benchmark_result->passed = false;
  benchmark_result->failure_reason = failure_reason;
}

static unsigned int hash_port_for_benchmark(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port port,
    unsigned int remaining_depth) {
  unsigned int tag = (unsigned int)get_port_tag(port);
  unsigned int value = (unsigned int)get_port_value(port);
  unsigned int hash_value = 2166136261u;
  hash_value = (hash_value ^ tag) * 16777619u;
  hash_value = (hash_value ^ value) * 16777619u;

  if (remaining_depth != 0 && is_binary_node_port(port) && value < MAXIMUM_NODE_COUNT) {
    PackedNode node = node_memory[value];
    hash_value ^= hash_port_for_benchmark(
        node_memory,
        get_first_auxiliary_port(node),
        remaining_depth - 1);
    hash_value *= 16777619u;
    hash_value ^= hash_port_for_benchmark(
        node_memory,
        get_second_auxiliary_port(node),
        remaining_depth - 1);
    hash_value *= 16777619u;
  }
  return hash_value;
}

static bool read_root_u24_output(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    unsigned int* observed_output) {
  Port root_output_port = substitution_memory[ROOT_VARIABLE_INDEX];
  if (get_port_tag(root_output_port) != PORT_TAG_NUMBER || !is_u24_number(root_output_port)) {
    return false;
  }
  *observed_output = (unsigned int)get_u24_from_numeric_port(root_output_port);
  return true;
}

static bool read_root_i24_output(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    int* observed_output) {
  Port root_output_port = substitution_memory[ROOT_VARIABLE_INDEX];
  if (get_port_tag(root_output_port) != PORT_TAG_NUMBER || !is_i24_number(root_output_port)) {
    return false;
  }
  *observed_output = (int)get_i24_from_numeric_port(root_output_port);
  return true;
}

static void fill_program_benchmark_result(
    ProgramBenchmarkResult* benchmark_result,
    ap_uint<32> performed_steps,
    ap_uint<32> active_pair_count,
    ap_uint<32> allocated_node_count,
    ap_uint<32> allocated_variable_count,
    ap_uint<32> status_code,
    ReducerStatistics* reducer_statistics) {
  benchmark_result->performed_steps = performed_steps;
  benchmark_result->final_active_pair_count = active_pair_count;
  benchmark_result->final_allocated_node_count = allocated_node_count;
  benchmark_result->final_allocated_variable_count = allocated_variable_count;
  benchmark_result->status_code = status_code;
  benchmark_result->statistics = *reducer_statistics;
}

static void run_program_book(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32>* active_pair_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    ProgramBenchmarkResult* benchmark_result) {
  ReducerStatistics reducer_statistics;
  clear_reducer_statistics(&reducer_statistics);
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;

  reducer_core(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &reducer_statistics,
      active_pair_count,
      allocated_node_count,
      allocated_variable_count,
      maximum_steps,
      &performed_steps,
      &status_code);

  fill_program_benchmark_result(
      benchmark_result,
      performed_steps,
      *active_pair_count,
      *allocated_node_count,
      *allocated_variable_count,
      status_code,
      &reducer_statistics);
}

static void run_parallel_program(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* active_pair_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    ProgramBenchmarkResult* benchmark_result) {
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      &book_definition_count);

  ReducerStatistics reducer_statistics;
  clear_reducer_statistics(&reducer_statistics);
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;
  reducer_core_parallel(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &reducer_statistics,
      active_pair_count,
      allocated_node_count,
      allocated_variable_count,
      maximum_steps,
      &performed_steps,
      &status_code);

  fill_program_benchmark_result(
      benchmark_result,
      performed_steps,
      *active_pair_count,
      *allocated_node_count,
      *allocated_variable_count,
      status_code,
      &reducer_statistics);
}

static bool build_numeric_u24_chain_program(
    ProgramBookBuilder* builder,
    const NumericTag operation_tags[16],
    const ap_uint<24> captured_operands[16],
    ap_uint<24> initial_value,
    unsigned int operation_count) {
  if (!program_book_begin_definition(builder, 0, 1, book_var(operation_count - 1), operation_count)) {
    return false;
  }
  for (unsigned int operation_index = 0; operation_index < operation_count; ++operation_index) {
    ap_uint<32> local_operator_node_index = 0;
    if (!program_book_add_node(
            builder,
            make_numeric_operation_port(operation_tags[operation_index], captured_operands[operation_index]),
            book_var(operation_index),
            &local_operator_node_index)) {
      return false;
    }
    Port active_input_port = operation_index == 0 ? book_u24(initial_value) : book_var(operation_index - 1);
    if (!program_book_add_redex(builder, active_input_port, book_ope(local_operator_node_index))) {
      return false;
    }
  }
  program_book_end_definition(builder);
  return program_book_succeeded(builder);
}

static bool build_numeric_i24_chain_program(
    ProgramBookBuilder* builder,
    const NumericTag operation_tags[16],
    const ap_int<24> captured_operands[16],
    ap_int<24> initial_value,
    unsigned int operation_count) {
  if (!program_book_begin_definition(builder, 0, 1, book_var(operation_count - 1), operation_count)) {
    return false;
  }
  for (unsigned int operation_index = 0; operation_index < operation_count; ++operation_index) {
    ap_uint<32> local_operator_node_index = 0;
    if (!program_book_add_node(
            builder,
            make_numeric_operation_port(
                operation_tags[operation_index],
                NumericPayload(captured_operands[operation_index])),
            book_var(operation_index),
            &local_operator_node_index)) {
      return false;
    }
    Port active_input_port = operation_index == 0 ? book_i24(initial_value) : book_var(operation_index - 1);
    if (!program_book_add_redex(builder, active_input_port, book_ope(local_operator_node_index))) {
      return false;
    }
  }
  program_book_end_definition(builder);
  return program_book_succeeded(builder);
}

static ProgramBenchmarkResult run_hvm_numeric_u24_chain_benchmark() {
#if ENABLE_NUMERIC_DIVISION && ENABLE_NUMERIC_REMAINDER
  ProgramBenchmarkResult result = {"hvm_numeric_u24_chain", 14, 1024, 0, false, 0, 0, 0, 0, 0, {}, ""};
#else
  ProgramBenchmarkResult result = {"hvm_numeric_u24_chain_small", 3, 4, 0, false, 0, 0, 0, 0, 0, {}, ""};
#endif
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

  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
#if ENABLE_NUMERIC_DIVISION && ENABLE_NUMERIC_REMAINDER
  NumericTag operation_tags[16] = {
      NUMERIC_TAG_OPERATION_ADD,
      NUMERIC_TAG_OPERATION_SUBTRACT,
      NUMERIC_TAG_OPERATION_MULTIPLY,
      NUMERIC_TAG_OPERATION_DIVIDE,
      NUMERIC_TAG_OPERATION_REMAINDER,
      NUMERIC_TAG_OPERATION_NOT_EQUAL,
      NUMERIC_TAG_OPERATION_EQUAL,
      NUMERIC_TAG_OPERATION_LESS_THAN,
      NUMERIC_TAG_OPERATION_GREATER_THAN,
      NUMERIC_TAG_OPERATION_AND,
      NUMERIC_TAG_OPERATION_OR,
      NUMERIC_TAG_OPERATION_XOR,
      NUMERIC_TAG_OPERATION_SHIFT_LEFT,
      NUMERIC_TAG_OPERATION_SHIFT_RIGHT};
  ap_uint<24> captured_operands[16] = {2, 20, 3, 120, 20, 7, 1, 1, 1, 3, 8, 15, 1, 1024};

  if (!build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 10, 14)) {
#else
  NumericTag operation_tags[16] = {
      NUMERIC_TAG_OPERATION_ADD,
      NUMERIC_TAG_OPERATION_MULTIPLY,
      NUMERIC_TAG_OPERATION_SUBTRACT};
  ap_uint<24> captured_operands[16] = {2, 3, 40};

  if (!build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 10, 3)) {
#endif
    fail_program_benchmark(&result, "program builder failed for U24 chain");
    return result;
  }
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      256,
      &result);

  if (!read_root_u24_output(substitution_memory, &result.observed_output)) {
    fail_program_benchmark(&result, "root output is not a U24 number");
  } else if (result.status_code != REDUCER_STATUS_SUCCESS || result.observed_output != result.expected_output) {
    fail_program_benchmark(&result, "U24 chain produced unexpected result or status");
  } else if (result.statistics.performed_operate_count < result.input_size ||
             result.statistics.performed_call_count != 1) {
    fail_program_benchmark(&result, "U24 chain did not exercise expected operate/call count");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_hvm_numeric_i24_chain_benchmark() {
#if ENABLE_NUMERIC_DIVISION && ENABLE_NUMERIC_REMAINDER
  unsigned int expected_output_bits = (unsigned int)(ap_uint<24>(ap_int<24>(-1)));
  ProgramBenchmarkResult result = {"hvm_numeric_i24_chain", 5, expected_output_bits, 0, false, 0, 0, 0, 0, 0, {}, ""};
#else
  unsigned int expected_output_bits = (unsigned int)(ap_uint<24>(ap_int<24>(1)));
  ProgramBenchmarkResult result = {"hvm_numeric_i24_chain_small", 3, expected_output_bits, 0, false, 0, 0, 0, 0, 0, {}, ""};
#endif
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

  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
#if ENABLE_NUMERIC_DIVISION && ENABLE_NUMERIC_REMAINDER
  NumericTag operation_tags[16] = {
      NUMERIC_TAG_OPERATION_ADD,
      NUMERIC_TAG_OPERATION_SUBTRACT,
      NUMERIC_TAG_OPERATION_MULTIPLY,
      NUMERIC_TAG_OPERATION_DIVIDE,
      NUMERIC_TAG_OPERATION_REMAINDER};
  ap_int<24> captured_operands[16] = {-5, 0, -3, 27, -4};

  if (!build_numeric_i24_chain_program(&builder, operation_tags, captured_operands, 2, 5)) {
#else
  NumericTag operation_tags[16] = {
      NUMERIC_TAG_OPERATION_ADD,
      NUMERIC_TAG_OPERATION_MULTIPLY,
      NUMERIC_TAG_OPERATION_SUBTRACT};
  ap_int<24> captured_operands[16] = {-5, -3, 10};

  if (!build_numeric_i24_chain_program(&builder, operation_tags, captured_operands, 2, 3)) {
#endif
    fail_program_benchmark(&result, "program builder failed for I24 chain");
    return result;
  }
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      256,
      &result);

  int observed_i24_output = 0;
  if (!read_root_i24_output(substitution_memory, &observed_i24_output)) {
    fail_program_benchmark(&result, "root output is not an I24 number");
  } else {
    result.observed_output = (unsigned int)(ap_uint<24>(ap_int<24>(observed_i24_output)));
    if (result.status_code != REDUCER_STATUS_SUCCESS ||
        (unsigned int)(ap_uint<24>(ap_int<24>(observed_i24_output))) != result.expected_output) {
      fail_program_benchmark(&result, "I24 chain produced unexpected result or status");
    } else {
      pass_program_benchmark(&result);
    }
  }
  return result;
}

static ProgramBenchmarkResult run_hvm_list_constructor_hash_benchmark() {
  ProgramBenchmarkResult result = {"hvm_list_constructor_hash", 3, 0, 0, false, 0, 0, 0, 0, 0, {}, ""};
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

  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  program_book_begin_definition(&builder, 0, 1, book_con(0), 0);
  ap_uint<32> ignored_node_index = 0;
  program_book_add_node(&builder, book_u24(1), book_con(1), &ignored_node_index);
  program_book_add_node(&builder, book_u24(2), book_con(2), &ignored_node_index);
  program_book_add_node(&builder, book_u24(3), make_port(PORT_TAG_ERASURE, 0), &ignored_node_index);
  program_book_end_definition(&builder);
  book_definition_count = builder.definition_count;

  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      16,
      &result);

  Port root_output_port = substitution_memory[ROOT_VARIABLE_INDEX];
  result.observed_output = hash_port_for_benchmark(node_memory, root_output_port, 8);
  result.expected_output = hash_port_for_benchmark(node_memory, make_port(PORT_TAG_CONSTRUCTOR, 0), 8);
  if (!program_book_succeeded(&builder)) {
    fail_program_benchmark(&result, "program builder failed for constructor hash program");
  } else if (result.status_code != REDUCER_STATUS_SUCCESS ||
             root_output_port != make_port(PORT_TAG_CONSTRUCTOR, 0)) {
    fail_program_benchmark(&result, "constructor program did not place expected root constructor");
  } else if (get_first_auxiliary_port(node_memory[0]) != book_u24(1) ||
             get_second_auxiliary_port(node_memory[0]) != make_port(PORT_TAG_CONSTRUCTOR, 1) ||
             get_first_auxiliary_port(node_memory[1]) != book_u24(2) ||
             get_second_auxiliary_port(node_memory[1]) != make_port(PORT_TAG_CONSTRUCTOR, 2) ||
             get_first_auxiliary_port(node_memory[2]) != book_u24(3)) {
    fail_program_benchmark(&result, "constructor/list shape does not match expected structure");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_hvm_sum_rec_scaled_benchmark() {
  ProgramBenchmarkResult result = {"hvm_sum_rec_scaled", 4, 120, 0, false, 0, 0, 0, 0, 0, {}, ""};
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

  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  NumericTag operation_tags[16];
  ap_uint<24> captured_operands[16];
  for (int operation_index = 0; operation_index < 15; ++operation_index) {
    operation_tags[operation_index] = NUMERIC_TAG_OPERATION_ADD;
    captured_operands[operation_index] = operation_index + 1;
  }
  build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 0, 15);
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      384,
      &result);

  if (!read_root_u24_output(substitution_memory, &result.observed_output)) {
    fail_program_benchmark(&result, "sum_rec root output is not U24");
  } else if (result.status_code != REDUCER_STATUS_SUCCESS || result.observed_output != result.expected_output) {
    fail_program_benchmark(&result, "sum_rec scaled result mismatch");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_hvm_sum_tree_scaled_benchmark() {
  ProgramBenchmarkResult result = {"hvm_sum_tree_scaled", 3, 28, 0, false, 0, 0, 0, 0, 0, {}, ""};
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

  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  program_book_begin_definition(&builder, 0, 1, book_con(4), 4);
  ap_uint<32> operator_node_index = 0;
  for (int pair_index = 0; pair_index < 4; ++pair_index) {
    program_book_add_node(
        &builder,
        make_numeric_operation_port(NUMERIC_TAG_OPERATION_ADD, pair_index * 2),
        book_var(pair_index),
        &operator_node_index);
    program_book_add_redex(&builder, book_u24((pair_index * 2) + 1), book_ope(operator_node_index));
  }
  program_book_add_node(&builder, book_var(0), book_con(5), &operator_node_index);
  program_book_add_node(&builder, book_var(1), book_con(6), &operator_node_index);
  program_book_add_node(&builder, book_var(2), book_var(3), &operator_node_index);
  program_book_end_definition(&builder);
  book_definition_count = builder.definition_count;

  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      384,
      &result);

  unsigned int observed_frontier_sum = 0;
  bool frontier_is_numeric = true;
  for (int variable_index = 0; variable_index < 4; ++variable_index) {
    Port frontier_port = substitution_memory[variable_index];
    frontier_is_numeric =
        frontier_is_numeric &&
        get_port_tag(frontier_port) == PORT_TAG_NUMBER &&
        is_u24_number(frontier_port);
    observed_frontier_sum += (unsigned int)get_u24_from_numeric_port(frontier_port);
  }
  result.observed_output = observed_frontier_sum;
  if (!frontier_is_numeric) {
    fail_program_benchmark(&result, "sum_tree frontier did not produce U24 leaves");
  } else if (result.status_code != REDUCER_STATUS_SUCCESS || result.observed_output != result.expected_output) {
    fail_program_benchmark(&result, "sum_tree scaled frontier sum mismatch");
  } else if (substitution_memory[ROOT_VARIABLE_INDEX] != make_port(PORT_TAG_CONSTRUCTOR, 4)) {
    fail_program_benchmark(&result, "sum_tree root constructor was not linked");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_countdown_loop_benchmark() {
  ProgramBenchmarkResult result = {"countdown_loop", 8, 0, 0, false, 0, 0, 0, 0, 0, {}, ""};
  NumericTag operation_tags[16];
  ap_uint<24> captured_operands[16];
  for (int operation_index = 0; operation_index < 8; ++operation_index) {
    operation_tags[operation_index] = NUMERIC_TAG_OPERATION_FLIPPED_SUBTRACT;
    captured_operands[operation_index] = 1;
  }

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
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 8, 8);
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      256,
      &result);
  if (!read_root_u24_output(substitution_memory, &result.observed_output) ||
      result.observed_output != result.expected_output ||
      result.status_code != REDUCER_STATUS_SUCCESS) {
    fail_program_benchmark(&result, "bounded countdown did not reach zero");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_sum_loop_benchmark() {
  ProgramBenchmarkResult result = {"sum_loop", 8, 36, 0, false, 0, 0, 0, 0, 0, {}, ""};
  NumericTag operation_tags[16];
  ap_uint<24> captured_operands[16];
  for (int operation_index = 0; operation_index < 8; ++operation_index) {
    operation_tags[operation_index] = NUMERIC_TAG_OPERATION_ADD;
    captured_operands[operation_index] = operation_index + 1;
  }

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
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 0, 8);
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      256,
      &result);
  if (!read_root_u24_output(substitution_memory, &result.observed_output) ||
      result.observed_output != result.expected_output ||
      result.status_code != REDUCER_STATUS_SUCCESS) {
    fail_program_benchmark(&result, "bounded sum loop produced wrong result");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_two_counter_transfer_benchmark() {
  ProgramBenchmarkResult result = {"two_counter_transfer", 7, 7, 0, false, 0, 0, 0, 0, 0, {}, ""};
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
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  NumericTag operation_tags[16] = {NUMERIC_TAG_OPERATION_ADD};
  ap_uint<24> captured_operands[16] = {3};
  build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 4, 1);
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      64,
      &result);
  if (!read_root_u24_output(substitution_memory, &result.observed_output) ||
      result.observed_output != result.expected_output ||
      result.status_code != REDUCER_STATUS_SUCCESS) {
    fail_program_benchmark(&result, "bounded two-counter transfer produced wrong total");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_hvm_stress_scaled_benchmark() {
  ProgramBenchmarkResult result = {"hvm_stress_scaled", 6, 21, 0, false, 0, 0, 0, 0, 0, {}, ""};
  NumericTag operation_tags[16];
  ap_uint<24> captured_operands[16];
  for (int operation_index = 0; operation_index < 6; ++operation_index) {
    operation_tags[operation_index] = NUMERIC_TAG_OPERATION_ADD;
    captured_operands[operation_index] = operation_index + 1;
  }

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
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  build_numeric_u24_chain_program(&builder, operation_tags, captured_operands, 0, 6);
  book_definition_count = builder.definition_count;
  active_pair_memory[0].left_port = book_ref(0);
  active_pair_memory[0].right_port = make_root_port();
  active_pair_memory[1].left_port = book_ref(0);
  active_pair_memory[1].right_port = book_u24(99);
  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      384,
      &result);
  if (!read_root_u24_output(substitution_memory, &result.observed_output) ||
      result.observed_output != result.expected_output ||
      result.statistics.performed_call_count < 2) {
    fail_program_benchmark(&result, "stress scaled program did not complete expected repeated calls");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_commute_fanout_depth_benchmark() {
  ProgramBenchmarkResult result = {"commute_fanout_depth", 4, 4, 0, false, 0, 0, 0, 0, 0, {}, ""};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  for (int pair_index = 0; pair_index < 4; ++pair_index) {
    ap_uint<1> left_success = 0;
    ap_uint<1> right_success = 0;
    NodeAddress left_node_address = allocate_node(
        node_memory,
        &allocated_node_count,
        make_port(PORT_TAG_VARIABLE, 10 + pair_index * 4),
        make_port(PORT_TAG_VARIABLE, 11 + pair_index * 4),
        &left_success);
    NodeAddress right_node_address = allocate_node(
        node_memory,
        &allocated_node_count,
        make_port(PORT_TAG_VARIABLE, 12 + pair_index * 4),
        make_port(PORT_TAG_VARIABLE, 13 + pair_index * 4),
        &right_success);
    active_pair_memory[pair_index].left_port = make_port(PORT_TAG_CONSTRUCTOR, left_node_address);
    active_pair_memory[pair_index].right_port = make_port(PORT_TAG_DUPLICATOR, right_node_address);
  }

  Chapter4ScenarioResult scenario_result = {"commute_fanout_depth", false, 0, 0, 0, 0, {}};
  ap_uint<32> active_pair_count = 4;
  run_reducer_scenario(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      4,
      &scenario_result);

  result.performed_steps = scenario_result.performed_steps;
  result.final_active_pair_count = scenario_result.final_redex_count;
  result.final_allocated_node_count = allocated_node_count;
  result.final_allocated_variable_count = allocated_variable_count;
  result.status_code = scenario_result.status_code;
  result.statistics = scenario_result.statistics;
  result.observed_output = (unsigned int)scenario_result.statistics.performed_commute_count;
  if (scenario_result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED ||
      scenario_result.statistics.performed_commute_count != 4) {
    fail_program_benchmark(&result, "commute fanout did not execute expected commute prefix");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_call_expansion_pressure_benchmark() {
  ProgramBenchmarkResult result = {"call_expansion_pressure", 6, 6, 0, false, 0, 0, 0, 0, 0, {}, ""};
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
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  program_book_begin_definition(&builder, 0, 1, book_var(0), 1);
  ap_uint<32> local_node_index = 0;
  program_book_add_node(&builder, book_u24(1), book_u24(2), &local_node_index);
  program_book_add_node(&builder, book_u24(3), book_u24(4), &local_node_index);
  program_book_add_node(&builder, book_u24(5), book_u24(6), &local_node_index);
  program_book_add_redex(&builder, book_con(0), book_con(1));
  program_book_add_redex(&builder, book_con(2), book_var(0));
  program_book_end_definition(&builder);
  book_definition_count = builder.definition_count;
  for (int call_index = 0; call_index < 6; ++call_index) {
    active_pair_memory[call_index].left_port = book_ref(0);
    active_pair_memory[call_index].right_port = book_u24(call_index);
  }
  ap_uint<32> active_pair_count = 6;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_program_book(
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
      6,
      &result);
  result.observed_output = (unsigned int)result.statistics.performed_call_count;
  if (result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED ||
      result.statistics.performed_call_count != 6 ||
      result.final_allocated_node_count != 18) {
    fail_program_benchmark(&result, "call pressure benchmark did not expand expected definitions");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_parallel_independent_void_batch_benchmark() {
  ProgramBenchmarkResult result = {"parallel_void_batch", 8, 8, 0, false, 0, 0, 0, 0, 0, {}, ""};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  for (int pair_index = 0; pair_index < 8; ++pair_index) {
    active_pair_memory[pair_index].left_port = make_port(PORT_TAG_ERASURE, 0);
    active_pair_memory[pair_index].right_port = book_u24(pair_index);
  }
  ap_uint<32> active_pair_count = 8;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_parallel_program(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      8,
      &result);
  result.observed_output = (unsigned int)result.statistics.performed_void_count;
  if (result.status_code != REDUCER_STATUS_SUCCESS ||
      result.statistics.performed_void_count != 8 ||
      result.statistics.dual_commit_cycle_count == 0) {
    fail_program_benchmark(&result, "parallel void batch did not use dual commits");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static ProgramBenchmarkResult run_parallel_mixed_contention_batch_benchmark() {
  ProgramBenchmarkResult result = {"parallel_mixed_contention", 4, 2, 0, false, 0, 0, 0, 0, 0, {}, ""};
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 7);
  active_pair_memory[0].right_port = book_u24(1);
  active_pair_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 7);
  active_pair_memory[1].right_port = book_u24(2);
  active_pair_memory[2].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[2].right_port = book_u24(3);
  active_pair_memory[3].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[3].right_port = book_u24(4);
  ap_uint<32> active_pair_count = 4;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  run_parallel_program(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      4,
      &result);
  result.observed_output = (unsigned int)result.statistics.substitution_conflict_count;
  if (result.status_code != REDUCER_STATUS_MAXIMUM_STEPS_REACHED ||
      result.statistics.substitution_conflict_count == 0 ||
      result.statistics.performed_link_count < 2) {
    fail_program_benchmark(&result, "parallel mixed workload did not expose substitution contention");
  } else {
    pass_program_benchmark(&result);
  }
  return result;
}

static bool test_program_book_builder_capacity() {
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(book_definition_headers, book_redex_memory, book_node_memory, &book_definition_count);
  ProgramBookBuilder builder;
  initialize_program_book_builder(&builder, book_definition_headers, book_redex_memory, book_node_memory);
  if (!program_book_begin_definition(&builder, 0, 1, book_con(0), 0)) {
    return false;
  }
  ap_uint<32> local_node_index = 0;
  ap_uint<1> last_add_success = 1;
  for (int node_index = 0; node_index < MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION + 1; ++node_index) {
    last_add_success = program_book_add_node(
        &builder,
        book_u24(node_index),
        make_port(PORT_TAG_ERASURE, 0),
        &local_node_index);
  }
  return last_add_success == 0 && !program_book_succeeded(&builder);
}
