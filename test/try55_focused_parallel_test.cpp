#include "reducer_test_common.hpp"
#include <iostream>

static bool run_try55_void_capture_test() {
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ReducerStatistics reducer_statistics;

  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  initialize_book_definition_headers(book_definition_headers);
  initialize_redex_memory(book_redex_memory);
  initialize_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  active_pair_memory[0].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[0].right_port = make_u24_number_port(0);
  active_pair_memory[1].left_port = make_port(PORT_TAG_ERASURE, 0);
  active_pair_memory[1].right_port = make_u24_number_port(1);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;

  reducer_core_parallel(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      0,
      &reducer_statistics,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &performed_steps,
      &status_code);

  return status_code == REDUCER_STATUS_SUCCESS &&
         performed_steps == 2 &&
         active_pair_count == 0;
}

static bool run_try55_link_capture_test() {
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ReducerStatistics reducer_statistics;

  initialize_node_memory(node_memory);
  initialize_substitution_memory(substitution_memory);
  initialize_redex_memory(active_pair_memory);
  initialize_book_definition_headers(book_definition_headers);
  initialize_redex_memory(book_redex_memory);
  initialize_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  active_pair_memory[0].left_port = make_port(PORT_TAG_VARIABLE, 21);
  active_pair_memory[0].right_port = make_port(PORT_TAG_REFERENCE, 210);
  active_pair_memory[1].left_port = make_port(PORT_TAG_VARIABLE, 22);
  active_pair_memory[1].right_port = make_port(PORT_TAG_REFERENCE, 220);

  ap_uint<32> active_pair_count = 2;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 32;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;

  reducer_core_parallel(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      0,
      &reducer_statistics,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      2,
      &performed_steps,
      &status_code);

  return status_code == REDUCER_STATUS_SUCCESS &&
         performed_steps == 2 &&
         active_pair_count == 0 &&
         substitution_memory[21] == make_port(PORT_TAG_REFERENCE, 210) &&
         substitution_memory[22] == make_port(PORT_TAG_REFERENCE, 220);
}

int main() {
  bool passed = true;
  std::cerr << "try55 void capture start\n";
  passed = passed && run_try55_void_capture_test();
  std::cerr << "try55 void capture done: " << passed << "\n";
  std::cerr << "try55 link capture start\n";
  passed = passed && run_try55_link_capture_test();
  std::cerr << "try55 link capture done: " << passed << "\n";
  return passed ? 0 : 1;
}
