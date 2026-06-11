#include "reducer_parallel_cosim_smoke.hpp"

#include "book_storage.hpp"
#include "numeric_encoding.hpp"

#ifndef LANE_BENCHMARK_PAIR_COUNT
#define LANE_BENCHMARK_PAIR_COUNT 16
#endif

static void initialize_smoke_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    node_memory[index] = 0;
  }
}

static void initialize_smoke_substitution_memory(Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port empty_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int index = 0; index < MAXIMUM_VARIABLE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    substitution_memory[index] = empty_port;
  }
}

static void initialize_smoke_redex_memory(RedexPair redex_memory[MAXIMUM_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    redex_memory[index].left_port = empty_port;
    redex_memory[index].right_port = empty_port;
  }
}

static void initialize_smoke_book_headers(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT]) {
  BookDefinitionHeader empty_header = make_empty_book_definition_header();
  for (int index = 0; index < MAXIMUM_BOOK_DEFINITION_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=16
    book_definition_headers[index] = empty_header;
  }
}

static void initialize_smoke_book_redex_memory(
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_BOOK_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_redex_memory[index].left_port = empty_port;
    book_redex_memory[index].right_port = empty_port;
  }
}

static void initialize_smoke_book_node_memory(
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_BOOK_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_node_memory[index] = 0;
  }
}

extern "C" void reducer_parallel_cosim_smoke(
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* parallel_cycle_count_out,
    ap_uint<32>* multi_commit_cycle_count_out,
    ap_uint<32>* performed_void_count_out,
    ap_uint<32>* maximum_global_queue_count_out) {
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ReducerStatistics reducer_statistics;

  initialize_smoke_node_memory(node_memory);
  initialize_smoke_substitution_memory(substitution_memory);
  initialize_smoke_redex_memory(active_pair_memory);
  initialize_smoke_book_headers(book_definition_headers);
  initialize_smoke_book_redex_memory(book_redex_memory);
  initialize_smoke_book_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  for (int pair_index = 0; pair_index < LANE_BENCHMARK_PAIR_COUNT; ++pair_index) {
#pragma HLS LOOP_TRIPCOUNT min=LANE_BENCHMARK_PAIR_COUNT max=LANE_BENCHMARK_PAIR_COUNT
    active_pair_memory[pair_index].left_port = make_port(PORT_TAG_ERASURE, 0);
    active_pair_memory[pair_index].right_port = make_u24_number_port(pair_index);
  }

  ap_uint<32> active_pair_count = LANE_BENCHMARK_PAIR_COUNT;
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
      LANE_BENCHMARK_PAIR_COUNT,
      &performed_steps,
      &status_code);

  *active_pair_count_out = active_pair_count;
  *performed_steps_out = performed_steps;
  *status_code_out = status_code;
  *parallel_cycle_count_out = reducer_statistics.parallel_cycle_count;
  *multi_commit_cycle_count_out = reducer_statistics.dual_commit_cycle_count;
  *performed_void_count_out = reducer_statistics.performed_void_count;
  *maximum_global_queue_count_out = reducer_statistics.maximum_observed_global_queue_count;
}
