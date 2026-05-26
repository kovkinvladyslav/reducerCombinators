#include "reducer_core_parallel.hpp"

static inline void record_parallel_error_status(
    ReducerStatistics* reducer_statistics,
    InteractionResult interaction_result,
    ap_uint<32>* local_status_code) {
  if (interaction_result.queue_overflow) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->redex_queue_overflow_count =
        reducer_statistics->redex_queue_overflow_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
  } else if (interaction_result.node_memory_exhausted) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->node_allocation_failure_count =
        reducer_statistics->node_allocation_failure_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_NODE_MEMORY_OVERFLOW;
  } else if (interaction_result.variable_memory_exhausted) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->variable_memory_overflow_count =
        reducer_statistics->variable_memory_overflow_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_VARIABLE_MEMORY_OVERFLOW;
  } else if (interaction_result.invalid_numeric_operation) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->invalid_numeric_operation_count =
        reducer_statistics->invalid_numeric_operation_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
  } else if (interaction_result.numeric_division_by_zero) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->numeric_division_by_zero_count =
        reducer_statistics->numeric_division_by_zero_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
  } else if (interaction_result.unsupported_numeric_type) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->invalid_numeric_operation_count =
        reducer_statistics->invalid_numeric_operation_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
  } else if (interaction_result.unknown_reference) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->unknown_reference_count =
        reducer_statistics->unknown_reference_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_UNKNOWN_REFERENCE;
  } else if (interaction_result.book_node_overflow) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->node_allocation_failure_count =
        reducer_statistics->node_allocation_failure_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_BOOK_NODE_OVERFLOW;
  } else if (interaction_result.book_redex_overflow) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->redex_queue_overflow_count =
        reducer_statistics->redex_queue_overflow_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_BOOK_REDEX_OVERFLOW;
  } else if (interaction_result.invalid_port_tag) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->unsupported_interaction_count =
        reducer_statistics->unsupported_interaction_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_INVALID_PORT_TAG;
  } else {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->unsupported_interaction_count =
        reducer_statistics->unsupported_interaction_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  }
}

static inline void increment_lane_committed_step_counter(
    ReducerStatistics* reducer_statistics,
    ap_uint<32> lane_index) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
  if (lane_index < REDUCER_LANE_COUNT) {
    reducer_statistics->lane_committed_step_count[lane_index] =
        reducer_statistics->lane_committed_step_count[lane_index] + 1;
  }
#endif
}

static inline void increment_lane_stall_counter(
    ReducerStatistics* reducer_statistics,
    ap_uint<32> lane_index) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
  if (lane_index < REDUCER_LANE_COUNT) {
    reducer_statistics->lane_stall_count[lane_index] =
        reducer_statistics->lane_stall_count[lane_index] + 1;
  }
#endif
}

static inline ap_uint<1> enqueue_produced_active_pairs_for_lane(
    ReducerLaneState* reducer_lane_state,
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value,
    ProducedActivePairs* produced_active_pairs,
    ReducerStatistics* reducer_statistics) {
  for (int produced_pair_index = 0;
       produced_pair_index < MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION;
       ++produced_pair_index) {
    if (produced_pair_index < produced_active_pairs->produced_pair_count) {
      RedexPair produced_active_pair = produced_active_pairs->produced_pairs[produced_pair_index];
      if (!active_pair_queue_push_local(reducer_lane_state, produced_active_pair)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->local_queue_spill_count =
            reducer_statistics->local_queue_spill_count + 1;
#endif
        if (!active_pair_queue_push_global(
                global_active_pair_memory,
                global_queue_tail_index,
                global_queue_count_value,
                produced_active_pair)) {
          return 0;
        }
      }
    }
  }
  return 1;
}

static inline ap_uint<1> commit_lane_active_pair(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerLaneState* reducer_lane_state,
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    RedexPair active_pair_to_commit,
    InteractionRuleKind interaction_rule_kind,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* local_status_code) {
  RedexPair discarded_active_pair;
  active_pair_queue_pop_local(reducer_lane_state, &discarded_active_pair);

  ProducedActivePairs produced_active_pairs;
  InteractionResult interaction_result = apply_lane_interaction_rule(
      node_memory,
      substitution_memory,
      &produced_active_pairs,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      allocated_node_count,
      allocated_variable_count,
      active_pair_to_commit.left_port,
      active_pair_to_commit.right_port,
      interaction_rule_kind,
      local_status_code,
      reducer_statistics);

  if (!interaction_result.success) {
    record_parallel_error_status(reducer_statistics, interaction_result, local_status_code);
    return 0;
  }

  if (!enqueue_produced_active_pairs_for_lane(
          reducer_lane_state,
          global_active_pair_memory,
          global_queue_tail_index,
          global_queue_count_value,
          &produced_active_pairs,
          reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->redex_queue_overflow_count =
        reducer_statistics->redex_queue_overflow_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
    return 0;
  }

#if ENABLE_REDUCER_STATISTICS
  count_executed_rule(reducer_statistics, interaction_rule_kind);
#endif
  return 1;
}

static inline ap_uint<1> lane_conflicts_with_committed_work(
    RedexPair candidate_active_pair,
    InteractionRuleKind candidate_rule_kind,
    RedexPair committed_active_pairs[REDUCER_LANE_COUNT],
    InteractionRuleKind committed_rule_kinds[REDUCER_LANE_COUNT],
    ap_uint<32> committed_active_pair_count,
    ReducerStatistics* reducer_statistics) {
  for (int committed_index = 0; committed_index < REDUCER_LANE_COUNT; ++committed_index) {
    if (committed_index < committed_active_pair_count) {
      RedexPair committed_active_pair = committed_active_pairs[committed_index];
      InteractionRuleKind committed_rule_kind = committed_rule_kinds[committed_index];

      if (candidate_rule_kind == INTERACTION_RULE_UNSUPPORTED ||
          committed_rule_kind == INTERACTION_RULE_UNSUPPORTED) {
        return 1;
      }

      if (candidate_rule_kind == INTERACTION_RULE_VOID &&
          committed_rule_kind == INTERACTION_RULE_VOID) {
        continue;
      }

      if (candidate_rule_kind == INTERACTION_RULE_LINK &&
          committed_rule_kind == INTERACTION_RULE_LINK &&
          get_port_value(get_link_variable_port_for_conflict_check(
              candidate_active_pair.left_port,
              candidate_active_pair.right_port)) ==
              get_port_value(get_link_variable_port_for_conflict_check(
                  committed_active_pair.left_port,
                  committed_active_pair.right_port))) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->substitution_conflict_count =
            reducer_statistics->substitution_conflict_count + 1;
#endif
        return 1;
      }

      if (candidate_rule_kind == INTERACTION_RULE_LINK &&
          committed_rule_kind == INTERACTION_RULE_LINK &&
          link_rules_touch_same_substitution_bank(candidate_active_pair, committed_active_pair)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->substitution_bank_conflict_count =
            reducer_statistics->substitution_bank_conflict_count + 1;
#endif
      }

      if (active_pairs_touch_same_binary_node(candidate_active_pair, committed_active_pair)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->node_access_conflict_count =
            reducer_statistics->node_access_conflict_count + 1;
#endif
        return 1;
      }

      if (active_pairs_touch_same_node_bank(candidate_active_pair, committed_active_pair)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->node_bank_conflict_count =
            reducer_statistics->node_bank_conflict_count + 1;
#endif
      }

      if (produced_rule_needs_allocation(candidate_rule_kind) ||
          produced_rule_needs_allocation(committed_rule_kind)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->allocation_contention_count =
            reducer_statistics->allocation_contention_count + 1;
#endif
        return 1;
      }
    }
  }
  return 0;
}

extern "C" void reducer_core_parallel(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* active_pair_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    ap_uint<32>* performed_steps,
    ap_uint<32>* status_code) {
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=substitution_memory cyclic factor=4 dim=1

#if ENABLE_REDUCER_STATISTICS
  clear_reducer_statistics(reducer_statistics);
#endif

  ReducerLaneState reducer_lanes[REDUCER_LANE_COUNT];
#pragma HLS ARRAY_PARTITION variable=reducer_lanes complete dim=1
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    initialize_reducer_lane_state(&reducer_lanes[lane_index]);
  }

  ap_uint<32> global_queue_head_index = 0;
  ap_uint<32> global_queue_tail_index = *active_pair_count;
  ap_uint<32> global_queue_count_value = *active_pair_count;
  ap_uint<32> executed_steps = 0;
  ap_uint<32> local_status_code = REDUCER_STATUS_SUCCESS;

  while (executed_steps < maximum_steps && local_status_code == REDUCER_STATUS_SUCCESS) {
#pragma HLS PIPELINE II=1
    ap_uint<1> any_local_work_available = 0;
    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      any_local_work_available =
          any_local_work_available || !active_pair_queue_is_local_empty(&reducer_lanes[lane_index]);
    }
    if (redex_queue_is_empty(global_queue_count_value) && !any_local_work_available) {
      break;
    }

#if ENABLE_PARALLEL_REDUCER_STATISTICS
    reducer_statistics->parallel_cycle_count =
        reducer_statistics->parallel_cycle_count + 1;
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_global_queue_count,
        global_queue_count_value);
    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      update_maximum_observed_counter(
          &reducer_statistics->maximum_observed_lane_queue_count[lane_index],
          reducer_lanes[lane_index].local_queue_count);
    }
#endif
#if ENABLE_REDUCER_STATISTICS
    ap_uint<32> total_observed_active_pair_count = global_queue_count_value;
    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      total_observed_active_pair_count =
          total_observed_active_pair_count + reducer_lanes[lane_index].local_queue_count;
    }
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_redex_queue_count,
        total_observed_active_pair_count);
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_allocated_node_count,
        *allocated_node_count);
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_allocated_variable_count,
        *allocated_variable_count);
#endif

    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      RedexPair refilled_active_pair;
      if (active_pair_queue_is_local_empty(&reducer_lanes[lane_index]) &&
          active_pair_queue_pop_global(
              global_active_pair_memory,
              &global_queue_head_index,
              &global_queue_count_value,
              &refilled_active_pair)) {
        active_pair_queue_push_local(&reducer_lanes[lane_index], refilled_active_pair);
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->global_refill_count = reducer_statistics->global_refill_count + 1;
#endif
      }
    }

    RedexPair candidate_active_pairs[REDUCER_LANE_COUNT];
    InteractionRuleKind candidate_rule_kinds[REDUCER_LANE_COUNT];
    ap_uint<1> lane_has_candidate[REDUCER_LANE_COUNT];
#pragma HLS ARRAY_PARTITION variable=candidate_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=candidate_rule_kinds complete dim=1
#pragma HLS ARRAY_PARTITION variable=lane_has_candidate complete dim=1
    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
      lane_has_candidate[lane_index] =
          active_pair_queue_peek_local(&reducer_lanes[lane_index], &candidate_active_pairs[lane_index]);
      candidate_rule_kinds[lane_index] = INTERACTION_RULE_UNSUPPORTED;
      if (lane_has_candidate[lane_index]) {
        candidate_rule_kinds[lane_index] = get_interaction_rule_kind(
            candidate_active_pairs[lane_index].left_port,
            candidate_active_pairs[lane_index].right_port);
      }
    }

    RedexPair committed_active_pairs[REDUCER_LANE_COUNT];
    InteractionRuleKind committed_rule_kinds[REDUCER_LANE_COUNT];
    ap_uint<32> committed_this_cycle_count = 0;
#pragma HLS ARRAY_PARTITION variable=committed_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=committed_rule_kinds complete dim=1

    for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
      if (lane_has_candidate[lane_index] && executed_steps < maximum_steps) {
        ap_uint<1> lane_can_commit_this_cycle =
            !lane_conflicts_with_committed_work(
                candidate_active_pairs[lane_index],
                candidate_rule_kinds[lane_index],
                committed_active_pairs,
                committed_rule_kinds,
                committed_this_cycle_count,
                reducer_statistics);

        if (lane_can_commit_this_cycle) {
          ap_uint<1> lane_committed = commit_lane_active_pair(
              node_memory,
              substitution_memory,
              global_active_pair_memory,
              book_definition_headers,
              book_redex_memory,
              book_node_memory,
              book_definition_count,
              &reducer_lanes[lane_index],
              &global_queue_tail_index,
              &global_queue_count_value,
              allocated_node_count,
              allocated_variable_count,
              candidate_active_pairs[lane_index],
              candidate_rule_kinds[lane_index],
              reducer_statistics,
              &local_status_code);
          if (!lane_committed) {
            break;
          }
          committed_active_pairs[committed_this_cycle_count] = candidate_active_pairs[lane_index];
          committed_rule_kinds[committed_this_cycle_count] = candidate_rule_kinds[lane_index];
          committed_this_cycle_count = committed_this_cycle_count + 1;
          executed_steps = executed_steps + 1;
          increment_lane_committed_step_counter(reducer_statistics, lane_index);
        } else {
          increment_lane_stall_counter(reducer_statistics, lane_index);
        }
      }
    }

#if ENABLE_PARALLEL_REDUCER_STATISTICS
    if (committed_this_cycle_count > 1) {
      reducer_statistics->dual_commit_cycle_count =
          reducer_statistics->dual_commit_cycle_count + 1;
    }
#endif
  }

  ap_uint<32> remaining_active_pair_count = global_queue_count_value;
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    remaining_active_pair_count =
        remaining_active_pair_count + reducer_lanes[lane_index].local_queue_count;
  }

  if (local_status_code == REDUCER_STATUS_SUCCESS &&
      executed_steps == maximum_steps &&
      remaining_active_pair_count != 0) {
    local_status_code = REDUCER_STATUS_MAXIMUM_STEPS_REACHED;
  }

  *active_pair_count = remaining_active_pair_count;
  *performed_steps = executed_steps;
  *status_code = local_status_code;
#if ENABLE_REDUCER_STATISTICS
  reducer_statistics->performed_step_count = executed_steps;
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_redex_queue_count,
      *active_pair_count);
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_allocated_node_count,
      *allocated_node_count);
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_allocated_variable_count,
      *allocated_variable_count);
#endif
}
