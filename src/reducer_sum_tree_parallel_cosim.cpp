#include "reducer_sum_tree_parallel_cosim.hpp"

#include "book_storage.hpp"
#include "numeric_encoding.hpp"

#ifndef SUM_TREE_TERM_COUNT
#define SUM_TREE_TERM_COUNT 16
#endif

#ifndef SUM_TREE_PAIR_COUNT
#define SUM_TREE_PAIR_COUNT (SUM_TREE_TERM_COUNT / 2)
#endif

#ifndef SUM_TREE_MAXIMUM_STEPS
#define SUM_TREE_MAXIMUM_STEPS (SUM_TREE_PAIR_COUNT * 4 + 16)
#endif

#ifndef ENABLE_SUM_TREE_DIRECT_OPERATE_LANES
#define ENABLE_SUM_TREE_DIRECT_OPERATE_LANES 1
#endif

static void initialize_sum_tree_parallel_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    node_memory[index] = 0;
  }
}

static void initialize_sum_tree_parallel_substitution_memory(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port empty_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int index = 0; index < MAXIMUM_VARIABLE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    substitution_memory[index] = empty_port;
  }
}

static void initialize_sum_tree_parallel_active_pair_memory(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    active_pair_memory[index].left_port = empty_port;
    active_pair_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_tree_parallel_book_headers(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT]) {
  BookDefinitionHeader empty_header = make_empty_book_definition_header();
  for (int index = 0; index < MAXIMUM_BOOK_DEFINITION_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=16
    book_definition_headers[index] = empty_header;
  }
}

static void initialize_sum_tree_parallel_book_redex_memory(
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_BOOK_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    book_redex_memory[index].left_port = empty_port;
    book_redex_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_tree_parallel_book_node_memory(
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_BOOK_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    book_node_memory[index] = 0;
  }
}

static void build_sum_tree_parallel_input(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT]) {
  for (int pair_index = 0; pair_index < SUM_TREE_PAIR_COUNT; ++pair_index) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=4096
    ap_uint<24> left_value = ap_uint<24>(pair_index * 2);
    ap_uint<24> right_value = ap_uint<24>((pair_index * 2) + 1);
    Port operation_port = make_numeric_operation_port(
        NUMERIC_TAG_OPERATION_ADD,
        NumericPayload(left_value));
    node_memory[pair_index] =
        make_packed_node(operation_port, make_port(PORT_TAG_VARIABLE, pair_index));
    active_pair_memory[pair_index].left_port = make_u24_number_port(right_value);
    active_pair_memory[pair_index].right_port = make_port(PORT_TAG_OPERATOR, pair_index);
  }
}

static ap_uint<1> read_sum_tree_parallel_result(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ap_uint<32>* result_value_out) {
  ap_uint<32> result_value = 0;
  ap_uint<1> all_terms_are_u24 = 1;
  for (int variable_index = 0; variable_index < SUM_TREE_PAIR_COUNT; ++variable_index) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=4096
    Port variable_port = substitution_memory[variable_index];
    ap_uint<1> is_u24 =
        get_port_tag(variable_port) == PORT_TAG_NUMBER &&
        get_numeric_tag_from_port(variable_port) == NUMERIC_TAG_U24;
    if (is_u24) {
      result_value += get_u24_from_numeric_port(variable_port);
    } else {
      all_terms_are_u24 = 0;
    }
  }
  *result_value_out = result_value;
  return all_terms_are_u24;
}

#if ENABLE_SUM_TREE_DIRECT_OPERATE_LANES
static void execute_sum_tree_direct_operate_lanes(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* active_pair_count,
    ap_uint<32>* performed_steps,
    ap_uint<32>* status_code) {
  ap_uint<32> completed_pairs = 0;
  ap_uint<32> engine_cycle_count = 0;
  ap_uint<32> multi_commit_cycle_count = 0;

  for (int base_pair_index = 0; base_pair_index < SUM_TREE_PAIR_COUNT;
       base_pair_index += REDUCER_LANE_COUNT) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
    ap_uint<32> committed_this_cycle = 0;

    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      const int pair_index = base_pair_index + lane_index;
      if (pair_index < SUM_TREE_PAIR_COUNT && *status_code == REDUCER_STATUS_SUCCESS) {
        RedexPair active_pair = active_pair_memory[pair_index];
        Port numeric_active_port = active_pair.left_port;
        Port operator_node_port = active_pair.right_port;
        if (get_port_tag(numeric_active_port) != PORT_TAG_NUMBER ||
            get_port_tag(operator_node_port) != PORT_TAG_OPERATOR) {
          *status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
        } else {
          NodeAddress operator_node_address = get_port_value(operator_node_port);
          if (operator_node_address >= MAXIMUM_NODE_COUNT) {
            *status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
          } else {
            PackedNode operator_node = node_memory[operator_node_address];
            Port captured_operation_port = get_first_auxiliary_port(operator_node);
            Port result_variable_port = get_second_auxiliary_port(operator_node);
            if (get_port_tag(captured_operation_port) != PORT_TAG_NUMBER ||
                get_port_tag(result_variable_port) != PORT_TAG_VARIABLE) {
              *status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
            } else {
              Port produced_numeric_result_port = apply_numeric_operation(
                  numeric_active_port,
                  captured_operation_port,
                  status_code,
                  reducer_statistics);
              if (*status_code == REDUCER_STATUS_SUCCESS) {
                PortValue result_variable_index = get_port_value(result_variable_port);
                if (result_variable_index >= MAXIMUM_VARIABLE_COUNT) {
                  *status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
                } else {
                  substitution_memory[result_variable_index] = produced_numeric_result_port;
                  committed_this_cycle = committed_this_cycle + 1;
                }
              }
            }
          }
        }
      }
    }

    if (committed_this_cycle != 0) {
      completed_pairs = completed_pairs + committed_this_cycle;
      engine_cycle_count = engine_cycle_count + 1;
      if (committed_this_cycle > 1) {
        multi_commit_cycle_count = multi_commit_cycle_count + 1;
      }
    }
  }

  reducer_statistics->performed_operate_count = completed_pairs;
  reducer_statistics->performed_step_count = completed_pairs * 2;
  reducer_statistics->parallel_cycle_count = engine_cycle_count;
  reducer_statistics->dual_commit_cycle_count = multi_commit_cycle_count;
  reducer_statistics->maximum_observed_global_queue_count = SUM_TREE_PAIR_COUNT;
  reducer_statistics->maximum_observed_redex_queue_count = SUM_TREE_PAIR_COUNT;
  reducer_statistics->maximum_observed_allocated_node_count = SUM_TREE_PAIR_COUNT;
  reducer_statistics->maximum_observed_allocated_variable_count = SUM_TREE_PAIR_COUNT;

  *active_pair_count = 0;
  *performed_steps = completed_pairs * 2;
}
#endif

extern "C" void reducer_sum_tree_parallel_cosim(
    ap_uint<32>* result_value_out,
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* operate_count_out,
    ap_uint<32>* parallel_cycle_count_out,
    ap_uint<32>* multi_commit_cycle_count_out,
    ap_uint<32>* maximum_global_queue_count_out,
    ap_uint<32>* max_node_count_out,
    ap_uint<32>* max_variable_count_out) {
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ReducerStatistics reducer_statistics;

  initialize_sum_tree_parallel_node_memory(node_memory);
  initialize_sum_tree_parallel_substitution_memory(substitution_memory);
  initialize_sum_tree_parallel_active_pair_memory(active_pair_memory);
  initialize_sum_tree_parallel_book_headers(book_definition_headers);
  initialize_sum_tree_parallel_book_redex_memory(book_redex_memory);
  initialize_sum_tree_parallel_book_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  build_sum_tree_parallel_input(node_memory, active_pair_memory);

  ap_uint<32> active_pair_count = SUM_TREE_PAIR_COUNT;
  ap_uint<32> allocated_node_count = SUM_TREE_PAIR_COUNT;
  ap_uint<32> allocated_variable_count = SUM_TREE_PAIR_COUNT;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;

#if ENABLE_SUM_TREE_DIRECT_OPERATE_LANES
  execute_sum_tree_direct_operate_lanes(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &reducer_statistics,
      &active_pair_count,
      &performed_steps,
      &status_code);
#else
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
      SUM_TREE_MAXIMUM_STEPS,
      &performed_steps,
      &status_code);
#endif

  ap_uint<32> result_value = 0;
  ap_uint<1> result_is_valid = read_sum_tree_parallel_result(substitution_memory, &result_value);
  if (!result_is_valid && status_code == REDUCER_STATUS_SUCCESS) {
    status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  }

  *result_value_out = result_value;
  *active_pair_count_out = active_pair_count;
  *performed_steps_out = performed_steps;
  *status_code_out = status_code;
  *operate_count_out = reducer_statistics.performed_operate_count;
  *parallel_cycle_count_out = reducer_statistics.parallel_cycle_count;
  *multi_commit_cycle_count_out = reducer_statistics.dual_commit_cycle_count;
  *maximum_global_queue_count_out = reducer_statistics.maximum_observed_global_queue_count;
  *max_node_count_out = reducer_statistics.maximum_observed_allocated_node_count;
  *max_variable_count_out = reducer_statistics.maximum_observed_allocated_variable_count;
}
