#include "parallel_reducer_live_demo_top.hpp"

#include "book_storage.hpp"
#include "numeric_encoding.hpp"
#include "reducer_core_parallel.hpp"

#ifndef LIVE_DEMO_HOLD_COUNT
#define LIVE_DEMO_HOLD_COUNT 32
#endif

static void initialize_live_demo_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    node_memory[index] = 0;
  }
}

static void initialize_live_demo_substitution_memory(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port empty_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int index = 0; index < MAXIMUM_VARIABLE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    substitution_memory[index] = empty_port;
  }
}

static void initialize_live_demo_active_pair_memory(
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=8192
    active_pair_memory[index].left_port = empty_port;
    active_pair_memory[index].right_port = empty_port;
  }
}

static ap_uint<1> read_live_demo_result(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ap_uint<32> input_pair_count,
    ap_uint<32>* result_value_out) {
  ap_uint<32> result_value = 0;
  ap_uint<1> all_terms_are_u24 = 1;
  for (int variable_index = 0; variable_index < MAXIMUM_VARIABLE_COUNT; ++variable_index) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=4096
    if (variable_index < input_pair_count) {
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
  }
  *result_value_out = result_value;
  return all_terms_are_u24;
}

static void write_channel(
    ap_uint<32> active_pair_number,
    ap_uint<32> operator_id,
    ap_uint<32> left_value,
    ap_uint<32> right_value,
    ap_uint<32> result_variable,
    volatile ap_uint<32>* active_pair_number_out,
    volatile ap_uint<32>* active_pair_operator_out,
    volatile ap_uint<32>* left_out,
    volatile ap_uint<32>* right_out,
    volatile ap_uint<32>* result_variable_out,
    volatile ap_uint<32>* result_out) {
  *active_pair_number_out = active_pair_number;
  *active_pair_operator_out = operator_id;
  *left_out = left_value;
  *right_out = right_value;
  *result_variable_out = result_variable;
  *result_out = left_value + right_value;
}

static void write_channel_snapshot(
    ap_uint<32> live_valid,
    ap_uint<32> live_cycle,
    const PreparedActivePairInput& channel0,
    const PreparedActivePairInput& channel1,
    const PreparedActivePairInput& channel2,
    const PreparedActivePairInput& channel3,
    volatile ap_uint<32>* live_valid_out,
    volatile ap_uint<32>* live_cycle_out,
    volatile ap_uint<32>* channel0_active_pair_number_out,
    volatile ap_uint<32>* channel0_active_pair_operator_out,
    volatile ap_uint<32>* channel0_left_out,
    volatile ap_uint<32>* channel0_right_out,
    volatile ap_uint<32>* channel0_result_variable_out,
    volatile ap_uint<32>* channel0_result_out,
    volatile ap_uint<32>* channel1_active_pair_number_out,
    volatile ap_uint<32>* channel1_active_pair_operator_out,
    volatile ap_uint<32>* channel1_left_out,
    volatile ap_uint<32>* channel1_right_out,
    volatile ap_uint<32>* channel1_result_variable_out,
    volatile ap_uint<32>* channel1_result_out,
    volatile ap_uint<32>* channel2_active_pair_number_out,
    volatile ap_uint<32>* channel2_active_pair_operator_out,
    volatile ap_uint<32>* channel2_left_out,
    volatile ap_uint<32>* channel2_right_out,
    volatile ap_uint<32>* channel2_result_variable_out,
    volatile ap_uint<32>* channel2_result_out,
    volatile ap_uint<32>* channel3_active_pair_number_out,
    volatile ap_uint<32>* channel3_active_pair_operator_out,
    volatile ap_uint<32>* channel3_left_out,
    volatile ap_uint<32>* channel3_right_out,
    volatile ap_uint<32>* channel3_result_variable_out,
    volatile ap_uint<32>* channel3_result_out) {
  *live_valid_out = live_valid;
  *live_cycle_out = live_cycle;
  write_channel(channel0.active_pair_number, channel0.operator_id, channel0.left_value,
                channel0.right_value, channel0.result_variable,
                channel0_active_pair_number_out, channel0_active_pair_operator_out,
                channel0_left_out, channel0_right_out, channel0_result_variable_out,
                channel0_result_out);
  write_channel(channel1.active_pair_number, channel1.operator_id, channel1.left_value,
                channel1.right_value, channel1.result_variable,
                channel1_active_pair_number_out, channel1_active_pair_operator_out,
                channel1_left_out, channel1_right_out, channel1_result_variable_out,
                channel1_result_out);
  write_channel(channel2.active_pair_number, channel2.operator_id, channel2.left_value,
                channel2.right_value, channel2.result_variable,
                channel2_active_pair_number_out, channel2_active_pair_operator_out,
                channel2_left_out, channel2_right_out, channel2_result_variable_out,
                channel2_result_out);
  write_channel(channel3.active_pair_number, channel3.operator_id, channel3.left_value,
                channel3.right_value, channel3.result_variable,
                channel3_active_pair_number_out, channel3_active_pair_operator_out,
                channel3_left_out, channel3_right_out, channel3_result_variable_out,
                channel3_result_out);
}

static void load_prepared_operate_pair(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32> pair_slot,
    const PreparedActivePairInput& prepared_pair) {
  if (pair_slot < MAXIMUM_REDEX_COUNT &&
      prepared_pair.operator_id < MAXIMUM_NODE_COUNT &&
      prepared_pair.result_variable < MAXIMUM_VARIABLE_COUNT) {
    Port operation_port = make_numeric_operation_port(
        NUMERIC_TAG_OPERATION_ADD,
        NumericPayload(ap_uint<24>(prepared_pair.left_value)));
    node_memory[prepared_pair.operator_id] = make_packed_node(
        operation_port,
        make_port(PORT_TAG_VARIABLE, prepared_pair.result_variable));
    active_pair_memory[pair_slot].left_port =
        make_u24_number_port(ap_uint<24>(prepared_pair.right_value));
    active_pair_memory[pair_slot].right_port =
        make_port(PORT_TAG_OPERATOR, prepared_pair.operator_id);
  }
}

static void execute_prepared_operate_pairs(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT],
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* active_pair_count,
    ap_uint<32>* performed_steps,
    ap_uint<32>* status_code,
    ap_uint<32> input_pair_count) {
  ap_uint<32> completed_pairs = 0;
  ap_uint<32> engine_cycle_count = 0;
  ap_uint<32> multi_commit_cycle_count = 0;

  for (int base_pair_index = 0; base_pair_index < MAXIMUM_REDEX_COUNT;
       base_pair_index += REDUCER_LANE_COUNT) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=4096
    ap_uint<32> committed_this_cycle = 0;

    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      const int pair_index = base_pair_index + lane_index;
      if (pair_index < input_pair_count && *status_code == REDUCER_STATUS_SUCCESS) {
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
    if ((base_pair_index + REDUCER_LANE_COUNT) >= input_pair_count) {
      break;
    }
  }

  reducer_statistics->performed_operate_count = completed_pairs;
  reducer_statistics->performed_step_count = completed_pairs * 2;
  reducer_statistics->parallel_cycle_count = engine_cycle_count;
  reducer_statistics->dual_commit_cycle_count = multi_commit_cycle_count;
  reducer_statistics->maximum_observed_global_queue_count = input_pair_count;
  reducer_statistics->maximum_observed_redex_queue_count = input_pair_count;
  reducer_statistics->maximum_observed_allocated_node_count = input_pair_count;
  reducer_statistics->maximum_observed_allocated_variable_count = input_pair_count;

  *active_pair_count = 0;
  *performed_steps = completed_pairs * 2;
}

void parallel_reducer_live_demo_top(
    const PreparedActivePairInput prepared_pairs[PREPARED_PAIR_COUNT],
    volatile ap_uint<32>* live_valid_out,
    volatile ap_uint<32>* live_cycle_out,
    volatile ap_uint<32>* channel0_active_pair_number_out,
    volatile ap_uint<32>* channel0_active_pair_operator_out,
    volatile ap_uint<32>* channel0_left_out,
    volatile ap_uint<32>* channel0_right_out,
    volatile ap_uint<32>* channel0_result_variable_out,
    volatile ap_uint<32>* channel0_result_out,
    volatile ap_uint<32>* channel1_active_pair_number_out,
    volatile ap_uint<32>* channel1_active_pair_operator_out,
    volatile ap_uint<32>* channel1_left_out,
    volatile ap_uint<32>* channel1_right_out,
    volatile ap_uint<32>* channel1_result_variable_out,
    volatile ap_uint<32>* channel1_result_out,
    volatile ap_uint<32>* channel2_active_pair_number_out,
    volatile ap_uint<32>* channel2_active_pair_operator_out,
    volatile ap_uint<32>* channel2_left_out,
    volatile ap_uint<32>* channel2_right_out,
    volatile ap_uint<32>* channel2_result_variable_out,
    volatile ap_uint<32>* channel2_result_out,
    volatile ap_uint<32>* channel3_active_pair_number_out,
    volatile ap_uint<32>* channel3_active_pair_operator_out,
    volatile ap_uint<32>* channel3_left_out,
    volatile ap_uint<32>* channel3_right_out,
    volatile ap_uint<32>* channel3_result_variable_out,
    volatile ap_uint<32>* channel3_result_out,
    volatile ap_uint<32>* final_result_out) {
#pragma HLS INTERFACE ap_ctrl_hs port=return
#pragma HLS INTERFACE ap_memory port=prepared_pairs

  *final_result_out = 0;

  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  ReducerStatistics reducer_statistics;
  initialize_live_demo_node_memory(node_memory);
  initialize_live_demo_substitution_memory(substitution_memory);
  initialize_live_demo_active_pair_memory(active_pair_memory);
  clear_reducer_statistics(&reducer_statistics);

  for (int pair_index = 0; pair_index < PREPARED_PAIR_COUNT; ++pair_index) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=8
    load_prepared_operate_pair(node_memory, active_pair_memory, pair_index,
                               prepared_pairs[pair_index]);
  }

  const PreparedActivePairInput empty_pair = {0, 0, 0, 0, 0};
  write_channel_snapshot(0, 0,
                         empty_pair, empty_pair, empty_pair, empty_pair,
                         live_valid_out, live_cycle_out,
                         channel0_active_pair_number_out, channel0_active_pair_operator_out,
                         channel0_left_out, channel0_right_out, channel0_result_variable_out,
                         channel0_result_out, channel1_active_pair_number_out,
                         channel1_active_pair_operator_out, channel1_left_out, channel1_right_out,
                         channel1_result_variable_out, channel1_result_out,
                         channel2_active_pair_number_out, channel2_active_pair_operator_out,
                         channel2_left_out, channel2_right_out, channel2_result_variable_out,
                         channel2_result_out, channel3_active_pair_number_out,
                         channel3_active_pair_operator_out, channel3_left_out, channel3_right_out,
                         channel3_result_variable_out, channel3_result_out);

  for (int hold_index = 0; hold_index < LIVE_DEMO_HOLD_COUNT; ++hold_index) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=32
    write_channel_snapshot(1, 1,
                           prepared_pairs[0], prepared_pairs[1],
                           prepared_pairs[2], prepared_pairs[3],
                           live_valid_out, live_cycle_out,
                           channel0_active_pair_number_out, channel0_active_pair_operator_out,
                           channel0_left_out, channel0_right_out, channel0_result_variable_out,
                           channel0_result_out, channel1_active_pair_number_out,
                           channel1_active_pair_operator_out, channel1_left_out,
                           channel1_right_out, channel1_result_variable_out, channel1_result_out,
                           channel2_active_pair_number_out, channel2_active_pair_operator_out,
                           channel2_left_out, channel2_right_out, channel2_result_variable_out,
                           channel2_result_out, channel3_active_pair_number_out,
                           channel3_active_pair_operator_out, channel3_left_out,
                           channel3_right_out, channel3_result_variable_out, channel3_result_out);
  }

  for (int hold_index = 0; hold_index < LIVE_DEMO_HOLD_COUNT; ++hold_index) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=32
    write_channel_snapshot(1, 2,
                           prepared_pairs[4], prepared_pairs[5],
                           prepared_pairs[6], prepared_pairs[7],
                           live_valid_out, live_cycle_out,
                           channel0_active_pair_number_out, channel0_active_pair_operator_out,
                           channel0_left_out, channel0_right_out, channel0_result_variable_out,
                           channel0_result_out, channel1_active_pair_number_out,
                           channel1_active_pair_operator_out, channel1_left_out,
                           channel1_right_out, channel1_result_variable_out, channel1_result_out,
                           channel2_active_pair_number_out, channel2_active_pair_operator_out,
                           channel2_left_out, channel2_right_out, channel2_result_variable_out,
                           channel2_result_out, channel3_active_pair_number_out,
                           channel3_active_pair_operator_out, channel3_left_out,
                           channel3_right_out, channel3_result_variable_out, channel3_result_out);
  }

  ap_uint<32> active_pair_count = PREPARED_PAIR_COUNT;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;
  execute_prepared_operate_pairs(
      node_memory,
      substitution_memory,
      active_pair_memory,
      &reducer_statistics,
      &active_pair_count,
      &performed_steps,
      &status_code,
      PREPARED_PAIR_COUNT);

  ap_uint<32> result_value = 0;
  ap_uint<1> result_is_valid =
      read_live_demo_result(substitution_memory, PREPARED_PAIR_COUNT, &result_value);
  if (status_code == REDUCER_STATUS_SUCCESS && result_is_valid) {
    *final_result_out = result_value;
  } else {
    *final_result_out = 0;
  }
}

