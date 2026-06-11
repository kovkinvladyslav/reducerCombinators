#include <iomanip>
#include <iostream>
#include <cmath>
#include <string>

#include "../src/reducer_core.hpp"
#include "../src/reducer_core_parallel.hpp"

struct Chapter4ScenarioResult {
  const char* test_name;
  bool passed;
  ap_uint<32> performed_steps;
  ap_uint<32> final_redex_count;
  ap_uint<32> final_allocated_node_count;
  ap_uint<32> status_code;
  ReducerStatistics statistics;
  const char* failure_reason;
};

struct ProgramBenchmarkResult {
  const char* program_name;
  unsigned int input_size;
  unsigned int expected_output;
  unsigned int observed_output;
  bool passed;
  ap_uint<32> performed_steps;
  ap_uint<32> final_active_pair_count;
  ap_uint<32> final_allocated_node_count;
  ap_uint<32> final_allocated_variable_count;
  ap_uint<32> status_code;
  ReducerStatistics statistics;
  const char* failure_reason;
};

template <typename T, unsigned N>
static unsigned array_length(const T (&)[N]) {
  return N;
}

static void pass_scenario(Chapter4ScenarioResult* scenario_result) {
  scenario_result->passed = true;
  scenario_result->failure_reason = "";
}

static void fail_scenario(Chapter4ScenarioResult* scenario_result, const char* failure_reason) {
  scenario_result->passed = false;
  scenario_result->failure_reason = failure_reason;
}

static bool require_allocation_success(ap_uint<1> allocation_successful) {
  return allocation_successful == 1;
}

static bool contains_active_pair(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    Port expected_left_port,
    Port expected_right_port) {
  for (unsigned int active_pair_index = 0; active_pair_index < scan_limit && active_pair_index < MAXIMUM_REDEX_COUNT; ++active_pair_index) {
    if (active_pair_memory[active_pair_index].left_port == expected_left_port &&
        active_pair_memory[active_pair_index].right_port == expected_right_port) {
      return true;
    }
  }
  return false;
}

static bool contains_active_pair_unordered(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    Port expected_first_port,
    Port expected_second_port) {
  return contains_active_pair(active_pair_memory, scan_limit, expected_first_port, expected_second_port) ||
         contains_active_pair(active_pair_memory, scan_limit, expected_second_port, expected_first_port);
}

static bool contains_u24_number_pair(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    unsigned int expected_value) {
  for (unsigned int active_pair_index = 0; active_pair_index < scan_limit && active_pair_index < MAXIMUM_REDEX_COUNT; ++active_pair_index) {
    Port candidate_ports[2] = {
        active_pair_memory[active_pair_index].left_port,
        active_pair_memory[active_pair_index].right_port};
    for (int candidate_port_index = 0; candidate_port_index < 2; ++candidate_port_index) {
      Port candidate_port = candidate_ports[candidate_port_index];
      if (get_port_tag(candidate_port) == PORT_TAG_NUMBER &&
          is_u24_number(candidate_port) &&
          get_u24_from_numeric_port(candidate_port) == expected_value) {
        return true;
      }
    }
  }
  return false;
}

static bool contains_i24_number_pair(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    int expected_value) {
  for (unsigned int active_pair_index = 0; active_pair_index < scan_limit && active_pair_index < MAXIMUM_REDEX_COUNT; ++active_pair_index) {
    Port candidate_ports[2] = {
        active_pair_memory[active_pair_index].left_port,
        active_pair_memory[active_pair_index].right_port};
    for (int candidate_port_index = 0; candidate_port_index < 2; ++candidate_port_index) {
      Port candidate_port = candidate_ports[candidate_port_index];
      if (get_port_tag(candidate_port) == PORT_TAG_NUMBER &&
          is_i24_number(candidate_port) &&
          get_i24_from_numeric_port(candidate_port) == expected_value) {
        return true;
      }
    }
  }
  return false;
}

static bool contains_f24_number_pair(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    float expected_value) {
  for (unsigned int active_pair_index = 0; active_pair_index < scan_limit && active_pair_index < MAXIMUM_REDEX_COUNT; ++active_pair_index) {
    Port candidate_ports[2] = {
        active_pair_memory[active_pair_index].left_port,
        active_pair_memory[active_pair_index].right_port};
    for (int candidate_port_index = 0; candidate_port_index < 2; ++candidate_port_index) {
      Port candidate_port = candidate_ports[candidate_port_index];
      if (get_port_tag(candidate_port) == PORT_TAG_NUMBER &&
          is_f24_number(candidate_port) &&
          std::fabs(get_f24_from_numeric_port(candidate_port) - expected_value) < 0.01f) {
        return true;
      }
    }
  }
  return false;
}

static bool find_active_pair_with_left_tag(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    unsigned int scan_limit,
    PortTag expected_left_tag,
    RedexPair* found_active_pair) {
  for (unsigned int active_pair_index = 0; active_pair_index < scan_limit && active_pair_index < MAXIMUM_REDEX_COUNT; ++active_pair_index) {
    if (get_port_tag(active_pair_memory[active_pair_index].left_port) == expected_left_tag) {
      *found_active_pair = active_pair_memory[active_pair_index];
      return true;
    }
  }
  return false;
}

static void initialize_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int node_index = 0; node_index < MAXIMUM_NODE_COUNT; ++node_index) {
    node_memory[node_index] = make_packed_node(make_port(PORT_TAG_ERASURE, 0), make_port(PORT_TAG_ERASURE, 0));
  }
}

static void initialize_substitution_memory(Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int variable_index = 0; variable_index < MAXIMUM_VARIABLE_COUNT; ++variable_index) {
    substitution_memory[variable_index] = unbound_marker;
  }
}

static void initialize_redex_memory(RedexPair redex_memory[MAXIMUM_REDEX_COUNT]) {
  for (int redex_index = 0; redex_index < MAXIMUM_REDEX_COUNT; ++redex_index) {
    redex_memory[redex_index].left_port = make_port(PORT_TAG_ERASURE, 0);
    redex_memory[redex_index].right_port = make_port(PORT_TAG_ERASURE, 0);
  }
}

static void initialize_book_definition_headers(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT]) {
  for (int definition_index = 0; definition_index < MAXIMUM_BOOK_DEFINITION_COUNT; ++definition_index) {
    book_definition_headers[definition_index] = make_empty_book_definition_header();
  }
}

static void initialize_book_redex_memory(
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT]) {
  for (int redex_index = 0; redex_index < MAXIMUM_BOOK_REDEX_COUNT; ++redex_index) {
    book_redex_memory[redex_index].left_port = make_port(PORT_TAG_ERASURE, 0);
    book_redex_memory[redex_index].right_port = make_port(PORT_TAG_ERASURE, 0);
  }
}

static void initialize_book_node_memory(
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  for (int node_index = 0; node_index < MAXIMUM_BOOK_NODE_COUNT; ++node_index) {
    book_node_memory[node_index] = make_packed_node(
        make_port(PORT_TAG_ERASURE, 0),
        make_port(PORT_TAG_ERASURE, 0));
  }
}

static void initialize_empty_book(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32>* book_definition_count) {
  initialize_book_definition_headers(book_definition_headers);
  initialize_book_redex_memory(book_redex_memory);
  initialize_book_node_memory(book_node_memory);
  *book_definition_count = 0;
}

static void run_reducer_scenario_with_book(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32>* redex_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    Chapter4ScenarioResult* scenario_result) {
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;
  clear_reducer_statistics(&scenario_result->statistics);

  reducer_core(
      node_memory,
      substitution_memory,
      redex_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &scenario_result->statistics,
      redex_count,
      allocated_node_count,
      allocated_variable_count,
      maximum_steps,
      &performed_steps,
      &status_code);

  scenario_result->performed_steps = performed_steps;
  scenario_result->final_redex_count = *redex_count;
  scenario_result->final_allocated_node_count = *allocated_node_count;
  scenario_result->status_code = status_code;
}

static void run_reducer_scenario(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    Chapter4ScenarioResult* scenario_result) {
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;
  clear_reducer_statistics(&scenario_result->statistics);
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      &book_definition_count);

  reducer_core(
      node_memory,
      substitution_memory,
      redex_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &scenario_result->statistics,
      redex_count,
      allocated_node_count,
      allocated_variable_count,
      maximum_steps,
      &performed_steps,
      &status_code);

  scenario_result->performed_steps = performed_steps;
  scenario_result->final_redex_count = *redex_count;
  scenario_result->final_allocated_node_count = *allocated_node_count;
  scenario_result->status_code = status_code;
}

static void run_parallel_reducer_scenario(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* active_pair_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    Chapter4ScenarioResult* scenario_result) {
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;
  clear_reducer_statistics(&scenario_result->statistics);
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ap_uint<32> book_definition_count = 0;
  initialize_empty_book(
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      &book_definition_count);

  reducer_core_parallel(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &scenario_result->statistics,
      active_pair_count,
      allocated_node_count,
      allocated_variable_count,
      maximum_steps,
      &performed_steps,
      &status_code);

  scenario_result->performed_steps = performed_steps;
  scenario_result->final_redex_count = *active_pair_count;
  scenario_result->final_allocated_node_count = *allocated_node_count;
  scenario_result->status_code = status_code;
}

static bool test_port_encoding() {
  for (int tag_index = PORT_TAG_VARIABLE; tag_index <= PORT_TAG_SWITCH; ++tag_index) {
    PortTag expected_tag = PortTag(tag_index);
    PortValue expected_value = PortValue((tag_index * 9) + 3);
    Port encoded_port = make_port(expected_tag, expected_value);
    if (get_port_tag(encoded_port) != expected_tag) return false;
    if (get_port_value(encoded_port) != expected_value) return false;
  }
  return true;
}

static bool test_node_packing() {
  Port first_auxiliary_port = make_port(PORT_TAG_CONSTRUCTOR, 123);
  Port second_auxiliary_port = make_port(PORT_TAG_DUPLICATOR, 456);
  PackedNode packed_node = make_packed_node(first_auxiliary_port, second_auxiliary_port);
  return get_first_auxiliary_port(packed_node) == first_auxiliary_port &&
         get_second_auxiliary_port(packed_node) == second_auxiliary_port;
}

static bool test_numeric_encoding() {
  Port u24_number_port = make_u24_number_port(22);
  Port i24_number_port = make_i24_number_port(-9);
  Port f24_number_port = make_f24_number_port(2.5f);
  Port operation_number_port = make_numeric_operation_port(NUMERIC_TAG_OPERATION_SUBTRACT, 10);

  if (get_numeric_tag_from_port(u24_number_port) != NUMERIC_TAG_U24) return false;
  if (get_u24_from_numeric_port(u24_number_port) != 22) return false;
  if (get_numeric_tag_from_port(i24_number_port) != NUMERIC_TAG_I24) return false;
  if (get_i24_from_numeric_port(i24_number_port) != -9) return false;
  if (get_numeric_tag_from_port(f24_number_port) != NUMERIC_TAG_F24) return false;
  if (get_numeric_tag_from_port(operation_number_port) != NUMERIC_TAG_OPERATION_SUBTRACT) return false;
  if (get_numeric_payload_from_port(operation_number_port) != 10) return false;
  return true;
}
