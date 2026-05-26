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

#if ENABLE_PARALLEL_PENDING_STATUS
static inline void defer_parallel_status(
    ap_uint<1>* pending_status_valid,
    ap_uint<32>* pending_status_code,
    ReducerServicePhase* parallel_service_phase,
    ap_uint<32> status_code) {
  *pending_status_valid = 1;
  *pending_status_code = status_code;
  *parallel_service_phase = PARALLEL_SERVICE_STATUS;
}
#endif

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

static inline ap_uint<1> execute_lane_active_pair_without_queue_commit(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerLaneState* reducer_lane_state,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    RedexPair active_pair_to_commit,
    InteractionRuleKind interaction_rule_kind,
    ProducedActivePairs* produced_active_pairs,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* local_status_code) {
  RedexPair discarded_active_pair;
  active_pair_queue_pop_local(reducer_lane_state, &discarded_active_pair);

  InteractionResult interaction_result = apply_lane_interaction_rule(
      node_memory,
      substitution_memory,
      produced_active_pairs,
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

#if ENABLE_REDUCER_STATISTICS
  count_executed_rule(reducer_statistics, interaction_rule_kind);
#endif
  return 1;
}

#if ENABLE_PARALLEL_STAGED_LINK
static inline ap_uint<1> execute_selected_non_link_active_pair_without_queue_commit(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerLaneState* reducer_lane_state,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    RedexPair active_pair_to_commit,
    InteractionRuleKind interaction_rule_kind,
    ProducedActivePairs* produced_active_pairs,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* local_status_code) {
  RedexPair discarded_active_pair;
  active_pair_queue_pop_local(reducer_lane_state, &discarded_active_pair);

  initialize_produced_active_pairs(produced_active_pairs);

  InteractionResult interaction_result = make_interaction_result(0);
  switch (interaction_rule_kind) {
    case INTERACTION_RULE_VOID:
      interaction_result = apply_void_rule();
      break;
#if ENABLE_RULE_ERASE
    case INTERACTION_RULE_ERASE:
      interaction_result = apply_lane_erase_rule(
          node_memory,
          produced_active_pairs,
          active_pair_to_commit.left_port,
          active_pair_to_commit.right_port);
      break;
#endif
#if ENABLE_RULE_ANNIHILATE
    case INTERACTION_RULE_ANNIHILATE:
      interaction_result = apply_lane_annihilate_rule(
          node_memory,
          produced_active_pairs,
          active_pair_to_commit.left_port,
          active_pair_to_commit.right_port);
      break;
#endif
#if ENABLE_RULE_COMMUTE || ENABLE_RULE_OPERATE || ENABLE_RULE_SWITCH || ENABLE_RULE_CALL
    default:
      interaction_result = apply_lane_interaction_rule(
          node_memory,
          substitution_memory,
          produced_active_pairs,
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
      break;
#else
    default:
      interaction_result.unsupported_interaction = 1;
      break;
#endif
  }

  if (!interaction_result.success) {
    record_parallel_error_status(reducer_statistics, interaction_result, local_status_code);
    return 0;
  }

#if ENABLE_REDUCER_STATISTICS
  count_executed_rule(reducer_statistics, interaction_rule_kind);
#endif
  return 1;
}
#endif

static inline ap_uint<1> commit_one_phased_produced_pair(
    ReducerLaneState* reducer_lane_state,
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value,
    ProducedActivePairs* produced_active_pairs,
    ProducedPairCount* produced_pair_index,
    ReducerStatistics* reducer_statistics) {
  if (*produced_pair_index >= produced_active_pairs->produced_pair_count) {
    return 1;
  }

  RedexPair produced_active_pair =
      produced_active_pairs->produced_pairs[*produced_pair_index];
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

  *produced_pair_index = *produced_pair_index + 1;
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

#if ENABLE_RULE_LINK
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
#endif

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

#if ENABLE_PARALLEL_PAIRWISE_SELECT || ENABLE_PARALLEL_TWO_COMMAND_BUFFER || ENABLE_PARALLEL_WAITING_COMMAND
static inline ap_uint<1> pair_ports_touch_same_binary_node(
    Port first_left_port,
    Port first_right_port,
    Port second_left_port,
    Port second_right_port) {
  ap_uint<1> first_left_is_binary = is_binary_node_port(first_left_port);
  ap_uint<1> first_right_is_binary = is_binary_node_port(first_right_port);
  ap_uint<1> second_left_is_binary = is_binary_node_port(second_left_port);
  ap_uint<1> second_right_is_binary = is_binary_node_port(second_right_port);

  PortValue first_left_value = get_port_value(first_left_port);
  PortValue first_right_value = get_port_value(first_right_port);
  PortValue second_left_value = get_port_value(second_left_port);
  PortValue second_right_value = get_port_value(second_right_port);

  return (first_left_is_binary && second_left_is_binary && first_left_value == second_left_value) ||
         (first_left_is_binary && second_right_is_binary && first_left_value == second_right_value) ||
         (first_right_is_binary && second_left_is_binary && first_right_value == second_left_value) ||
         (first_right_is_binary && second_right_is_binary && first_right_value == second_right_value);
}

static inline ap_uint<1> pair_ports_touch_same_node_bank(
    Port first_left_port,
    Port first_right_port,
    Port second_left_port,
    Port second_right_port) {
  ap_uint<1> first_left_is_binary = is_binary_node_port(first_left_port);
  ap_uint<1> first_right_is_binary = is_binary_node_port(first_right_port);
  ap_uint<1> second_left_is_binary = is_binary_node_port(second_left_port);
  ap_uint<1> second_right_is_binary = is_binary_node_port(second_right_port);

  PortValue first_left_value = get_port_value(first_left_port);
  PortValue first_right_value = get_port_value(first_right_port);
  PortValue second_left_value = get_port_value(second_left_port);
  PortValue second_right_value = get_port_value(second_right_port);

  return (first_left_is_binary && second_left_is_binary &&
          first_left_value != second_left_value &&
          get_node_memory_bank_index(first_left_port) == get_node_memory_bank_index(second_left_port)) ||
         (first_left_is_binary && second_right_is_binary &&
          first_left_value != second_right_value &&
          get_node_memory_bank_index(first_left_port) == get_node_memory_bank_index(second_right_port)) ||
         (first_right_is_binary && second_left_is_binary &&
          first_right_value != second_left_value &&
          get_node_memory_bank_index(first_right_port) == get_node_memory_bank_index(second_left_port)) ||
         (first_right_is_binary && second_right_is_binary &&
          first_right_value != second_right_value &&
          get_node_memory_bank_index(first_right_port) == get_node_memory_bank_index(second_right_port));
}

static inline ap_uint<1> pairwise_active_pairs_conflict(
    RedexPair candidate_active_pair,
    InteractionRuleKind candidate_rule_kind,
    RedexPair committed_active_pair,
    InteractionRuleKind committed_rule_kind,
    ReducerStatistics* reducer_statistics) {
  if (candidate_rule_kind == INTERACTION_RULE_UNSUPPORTED ||
      committed_rule_kind == INTERACTION_RULE_UNSUPPORTED) {
    return 1;
  }

  if (candidate_rule_kind == INTERACTION_RULE_VOID &&
      committed_rule_kind == INTERACTION_RULE_VOID) {
    return 0;
  }

#if ENABLE_RULE_LINK
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
#endif

  if (pair_ports_touch_same_binary_node(
          candidate_active_pair.left_port,
          candidate_active_pair.right_port,
          committed_active_pair.left_port,
          committed_active_pair.right_port)) {
#if ENABLE_PARALLEL_REDUCER_STATISTICS
    reducer_statistics->node_access_conflict_count =
        reducer_statistics->node_access_conflict_count + 1;
#endif
    return 1;
  }

  if (pair_ports_touch_same_node_bank(
          candidate_active_pair.left_port,
          candidate_active_pair.right_port,
          committed_active_pair.left_port,
          committed_active_pair.right_port)) {
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

  return 0;
}
#endif

#if ENABLE_PARALLEL_SERVICE_HELPERS
static ap_uint<1> parallel_refill_service(
    ReducerLaneState reducer_lanes[REDUCER_LANE_COUNT],
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_head_index,
    ap_uint<32>* global_queue_count_value,
    ReducerStatistics* reducer_statistics) {
#pragma HLS INLINE off
  ap_uint<1> any_local_work_available = 0;
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    any_local_work_available =
        any_local_work_available || !active_pair_queue_is_local_empty(&reducer_lanes[lane_index]);
  }
  if (redex_queue_is_empty(*global_queue_count_value) && !any_local_work_available) {
    return 0;
  }

  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    RedexPair refilled_active_pair;
    if (active_pair_queue_is_local_empty(&reducer_lanes[lane_index]) &&
        active_pair_queue_pop_global(
            global_active_pair_memory,
            global_queue_head_index,
            global_queue_count_value,
            &refilled_active_pair)) {
      active_pair_queue_push_local(&reducer_lanes[lane_index], refilled_active_pair);
#if ENABLE_PARALLEL_REDUCER_STATISTICS
      reducer_statistics->global_refill_count = reducer_statistics->global_refill_count + 1;
#endif
    }
  }
  return 1;
}

static ap_uint<1> parallel_select_service(
    ReducerLaneState reducer_lanes[REDUCER_LANE_COUNT],
    ap_uint<32> executed_steps,
    ap_uint<32> maximum_steps,
    RedexPair phased_candidate_active_pairs[REDUCER_LANE_COUNT],
    InteractionRuleKind phased_candidate_rule_kinds[REDUCER_LANE_COUNT],
    ap_uint<1> phased_lane_should_commit[REDUCER_LANE_COUNT],
    ProducedActivePairs phased_produced_active_pairs[REDUCER_LANE_COUNT],
    ProducedPairCount phased_produced_pair_index[REDUCER_LANE_COUNT],
    ReducerStatistics* reducer_statistics) {
#pragma HLS INLINE off
  RedexPair committed_active_pairs[REDUCER_LANE_COUNT];
  InteractionRuleKind committed_rule_kinds[REDUCER_LANE_COUNT];
  ap_uint<32> committed_this_cycle_count = 0;
#pragma HLS ARRAY_PARTITION variable=committed_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=committed_rule_kinds complete dim=1

  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    phased_lane_should_commit[lane_index] = 0;
    phased_candidate_rule_kinds[lane_index] = INTERACTION_RULE_UNSUPPORTED;
    initialize_produced_active_pairs(&phased_produced_active_pairs[lane_index]);
    phased_produced_pair_index[lane_index] = 0;

    ap_uint<1> lane_has_candidate =
        active_pair_queue_peek_local(&reducer_lanes[lane_index], &phased_candidate_active_pairs[lane_index]);
    if (lane_has_candidate && executed_steps + committed_this_cycle_count < maximum_steps) {
      phased_candidate_rule_kinds[lane_index] = get_interaction_rule_kind(
          phased_candidate_active_pairs[lane_index].left_port,
          phased_candidate_active_pairs[lane_index].right_port);
      ap_uint<1> lane_can_commit_this_cycle =
          !lane_conflicts_with_committed_work(
              phased_candidate_active_pairs[lane_index],
              phased_candidate_rule_kinds[lane_index],
              committed_active_pairs,
              committed_rule_kinds,
              committed_this_cycle_count,
              reducer_statistics);
      if (lane_can_commit_this_cycle) {
        phased_lane_should_commit[lane_index] = 1;
        committed_active_pairs[committed_this_cycle_count] = phased_candidate_active_pairs[lane_index];
        committed_rule_kinds[committed_this_cycle_count] = phased_candidate_rule_kinds[lane_index];
        committed_this_cycle_count = committed_this_cycle_count + 1;
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
  return committed_this_cycle_count != 0;
}

static void parallel_execute_service(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerLaneState reducer_lanes[REDUCER_LANE_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    RedexPair phased_candidate_active_pairs[REDUCER_LANE_COUNT],
    InteractionRuleKind phased_candidate_rule_kinds[REDUCER_LANE_COUNT],
    ap_uint<1> phased_lane_should_commit[REDUCER_LANE_COUNT],
    ProducedActivePairs phased_produced_active_pairs[REDUCER_LANE_COUNT],
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* executed_steps,
    ap_uint<32>* local_status_code) {
#pragma HLS INLINE off
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    if (phased_lane_should_commit[lane_index] &&
        *local_status_code == REDUCER_STATUS_SUCCESS) {
      ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
          node_memory,
          substitution_memory,
          book_definition_headers,
          book_redex_memory,
          book_node_memory,
          book_definition_count,
          &reducer_lanes[lane_index],
          allocated_node_count,
          allocated_variable_count,
          phased_candidate_active_pairs[lane_index],
          phased_candidate_rule_kinds[lane_index],
          &phased_produced_active_pairs[lane_index],
          reducer_statistics,
          local_status_code);
      if (lane_committed) {
        *executed_steps = *executed_steps + 1;
        increment_lane_committed_step_counter(reducer_statistics, lane_index);
      }
    }
  }
}

static void parallel_commit_service(
    ReducerLaneState reducer_lanes[REDUCER_LANE_COUNT],
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value,
    ap_uint<1> phased_lane_should_commit[REDUCER_LANE_COUNT],
    ProducedActivePairs phased_produced_active_pairs[REDUCER_LANE_COUNT],
    ProducedPairCount phased_produced_pair_index[REDUCER_LANE_COUNT],
    ReducerStatistics* reducer_statistics,
    ReducerServicePhase* parallel_service_phase,
    ap_uint<32>* local_status_code) {
#pragma HLS INLINE off
  ap_uint<1> any_pending_produced_pair = 0;
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    if (phased_lane_should_commit[lane_index] &&
        phased_produced_pair_index[lane_index] <
            phased_produced_active_pairs[lane_index].produced_pair_count) {
      any_pending_produced_pair = 1;
    }
  }

  if (!any_pending_produced_pair) {
    *parallel_service_phase = PARALLEL_SERVICE_REFILL;
    return;
  }

  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
    if (phased_lane_should_commit[lane_index] &&
        phased_produced_pair_index[lane_index] <
            phased_produced_active_pairs[lane_index].produced_pair_count &&
        *local_status_code == REDUCER_STATUS_SUCCESS) {
      if (!commit_one_phased_produced_pair(
              &reducer_lanes[lane_index],
              global_active_pair_memory,
              global_queue_tail_index,
              global_queue_count_value,
              &phased_produced_active_pairs[lane_index],
              &phased_produced_pair_index[lane_index],
              reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
        reducer_statistics->redex_queue_overflow_count =
            reducer_statistics->redex_queue_overflow_count + 1;
#endif
        *local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
      }
      break;
    }
  }
}
#endif

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
#if ENABLE_REGISTER_LANE_QUEUE
#pragma HLS ARRAY_PARTITION variable=reducer_lanes->local_active_pair_queue complete dim=1
#endif
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    initialize_reducer_lane_state(&reducer_lanes[lane_index]);
  }

  ap_uint<32> global_queue_head_index = 0;
  ap_uint<32> global_queue_tail_index = *active_pair_count;
  ap_uint<32> global_queue_count_value = *active_pair_count;
  ap_uint<32> executed_steps = 0;
  ap_uint<32> local_status_code = REDUCER_STATUS_SUCCESS;
#if ENABLE_PARALLEL_SERVICE_PHASE
  ReducerServicePhase parallel_service_phase = PARALLEL_SERVICE_REFILL;
#if ENABLE_PARALLEL_SPLIT_REFILL && !ENABLE_PARALLEL_SINGLE_REFILL_POP
  ap_uint<1> refill_lane_needs_work[REDUCER_LANE_COUNT];
#endif
#if ENABLE_PARALLEL_SPLIT_REFILL
#if ENABLE_PARALLEL_SINGLE_REFILL_POP
  ap_uint<1> refill_target_lane = 0;
#endif
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
  ap_uint<1> pending_status_valid = 0;
  ap_uint<32> pending_status_code = REDUCER_STATUS_SUCCESS;
#endif
  RedexPair phased_candidate_active_pairs[REDUCER_LANE_COUNT];
  InteractionRuleKind phased_candidate_rule_kinds[REDUCER_LANE_COUNT];
  ap_uint<1> phased_lane_should_commit[REDUCER_LANE_COUNT];
  ProducedActivePairs phased_produced_active_pairs[REDUCER_LANE_COUNT];
  ProducedPairCount phased_produced_pair_index[REDUCER_LANE_COUNT];
#if ENABLE_PARALLEL_SELECTED_COMMAND
  ap_uint<1> selected_command_valid = 0;
  ap_uint<1> selected_command_lane_index = 0;
  RedexPair selected_command_active_pair;
  InteractionRuleKind selected_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;
  ProducedActivePairs selected_command_produced_active_pairs;
  ProducedPairCount selected_command_produced_pair_index = 0;
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
  ap_uint<1> selected_command_completion_valid = 0;
  ap_uint<1> selected_command_completion_lane_index = 0;
  InteractionRuleKind selected_command_completion_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
#if ENABLE_PARALLEL_STAGED_LINK
  ap_uint<1> selected_command_link_active = 0;
#if ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT
  ap_uint<3> selected_command_link_stage = PARALLEL_LINK_STAGE_IDLE;
#else
  ap_uint<2> selected_command_link_stage = PARALLEL_LINK_STAGE_IDLE;
#endif
  PortValue selected_command_link_variable_index = 0;
  Port selected_command_link_other_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  Port selected_command_link_previous_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
#if ENABLE_PARALLEL_LINK_READ_DECODE
  ap_uint<1> selected_command_link_previous_is_unbound = 0;
#endif
#if ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
  ap_uint<1> selected_command_link_emit_pair = 0;
#endif
#endif
#endif
#if ENABLE_PARALLEL_TWO_COMMAND_BUFFER
  ap_uint<1> command0_valid = 0;
  ap_uint<1> command0_lane_index = 0;
  RedexPair command0_active_pair;
  InteractionRuleKind command0_rule_kind = INTERACTION_RULE_UNSUPPORTED;
  ProducedActivePairs command0_produced_active_pairs;
  ProducedPairCount command0_produced_pair_index = 0;

  ap_uint<1> command1_valid = 0;
  ap_uint<1> command1_lane_index = 0;
  RedexPair command1_active_pair;
  InteractionRuleKind command1_rule_kind = INTERACTION_RULE_UNSUPPORTED;
  ProducedActivePairs command1_produced_active_pairs;
  ProducedPairCount command1_produced_pair_index = 0;
#endif
#if ENABLE_PARALLEL_WAITING_COMMAND
  ap_uint<1> active_command_valid = 0;
  ap_uint<1> active_command_executed = 0;
  ap_uint<1> active_command_lane_index = 0;
  RedexPair active_command_active_pair;
  InteractionRuleKind active_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;
  ProducedActivePairs active_command_produced_active_pairs;
  ProducedPairCount active_command_produced_pair_index = 0;

  ap_uint<1> waiting_command_valid = 0;
  ap_uint<1> waiting_command_lane_index = 0;
  RedexPair waiting_command_active_pair;
  InteractionRuleKind waiting_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
#pragma HLS ARRAY_PARTITION variable=phased_candidate_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=phased_candidate_rule_kinds complete dim=1
#pragma HLS ARRAY_PARTITION variable=phased_lane_should_commit complete dim=1
#pragma HLS ARRAY_PARTITION variable=phased_produced_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=phased_produced_pair_index complete dim=1
#if ENABLE_PARALLEL_SPLIT_REFILL && !ENABLE_PARALLEL_SINGLE_REFILL_POP
#pragma HLS ARRAY_PARTITION variable=refill_lane_needs_work complete dim=1
#endif
#if ENABLE_PARALLEL_SELECTED_COMMAND
#pragma HLS ARRAY_PARTITION variable=selected_command_produced_active_pairs.produced_pairs complete dim=1
  initialize_produced_active_pairs(&selected_command_produced_active_pairs);
#endif
#if ENABLE_PARALLEL_TWO_COMMAND_BUFFER
#pragma HLS ARRAY_PARTITION variable=command0_produced_active_pairs.produced_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=command1_produced_active_pairs.produced_pairs complete dim=1
  initialize_produced_active_pairs(&command0_produced_active_pairs);
  initialize_produced_active_pairs(&command1_produced_active_pairs);
#endif
#if ENABLE_PARALLEL_WAITING_COMMAND
#pragma HLS ARRAY_PARTITION variable=active_command_produced_active_pairs.produced_pairs complete dim=1
  initialize_produced_active_pairs(&active_command_produced_active_pairs);
#endif
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
    phased_candidate_rule_kinds[lane_index] = INTERACTION_RULE_UNSUPPORTED;
    phased_lane_should_commit[lane_index] = 0;
    phased_produced_pair_index[lane_index] = 0;
#if ENABLE_PARALLEL_SPLIT_REFILL && !ENABLE_PARALLEL_SINGLE_REFILL_POP
    refill_lane_needs_work[lane_index] = 0;
#endif
    initialize_produced_active_pairs(&phased_produced_active_pairs[lane_index]);
  }
#endif

  while (
#if ENABLE_PARALLEL_SERVICE_PHASE
      (executed_steps < maximum_steps ||
       parallel_service_phase == PARALLEL_SERVICE_COMMIT
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
       || parallel_service_phase == PARALLEL_SERVICE_COMPLETE_SELECTED
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
       || parallel_service_phase == PARALLEL_SERVICE_STATUS
#endif
       ) &&
#else
      executed_steps < maximum_steps &&
#endif
      local_status_code == REDUCER_STATUS_SUCCESS) {
#if !ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH
#pragma HLS PIPELINE II=1
#endif
#if ENABLE_PARALLEL_SERVICE_PHASE
#if ENABLE_PARALLEL_SERVICE_HELPERS
    if (parallel_service_phase == PARALLEL_SERVICE_REFILL) {
      if (!parallel_refill_service(
              reducer_lanes,
              global_active_pair_memory,
              &global_queue_head_index,
              &global_queue_count_value,
              reducer_statistics)) {
        break;
      }
      parallel_service_phase = PARALLEL_SERVICE_SELECT;
    } else if (parallel_service_phase == PARALLEL_SERVICE_SELECT) {
      if (parallel_select_service(
              reducer_lanes,
              executed_steps,
              maximum_steps,
              phased_candidate_active_pairs,
              phased_candidate_rule_kinds,
              phased_lane_should_commit,
              phased_produced_active_pairs,
              phased_produced_pair_index,
              reducer_statistics)) {
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      }
    } else if (parallel_service_phase == PARALLEL_SERVICE_EXECUTE) {
      parallel_execute_service(
          node_memory,
          substitution_memory,
          book_definition_headers,
          book_redex_memory,
          book_node_memory,
          book_definition_count,
          reducer_lanes,
          allocated_node_count,
          allocated_variable_count,
          phased_candidate_active_pairs,
          phased_candidate_rule_kinds,
          phased_lane_should_commit,
          phased_produced_active_pairs,
          reducer_statistics,
          &executed_steps,
          &local_status_code);
      parallel_service_phase = PARALLEL_SERVICE_COMMIT;
    } else {
      parallel_commit_service(
          reducer_lanes,
          global_active_pair_memory,
          &global_queue_tail_index,
          &global_queue_count_value,
          phased_lane_should_commit,
          phased_produced_active_pairs,
          phased_produced_pair_index,
          reducer_statistics,
          &parallel_service_phase,
          &local_status_code);
    }
#else
    if (parallel_service_phase == PARALLEL_SERVICE_REFILL) {
#if ENABLE_PARALLEL_SPLIT_REFILL
#if ENABLE_PARALLEL_SINGLE_REFILL_POP
      ap_uint<1> lane0_empty = active_pair_queue_is_local_empty(&reducer_lanes[0]);
      ap_uint<1> lane1_empty = active_pair_queue_is_local_empty(&reducer_lanes[1]);
      ap_uint<1> any_local_work_available = !lane0_empty || !lane1_empty;
      ap_uint<1> any_lane_needs_refill = lane0_empty || lane1_empty;
      refill_target_lane = lane0_empty ? (ap_uint<1>)0 : (ap_uint<1>)1;

      if (redex_queue_is_empty(global_queue_count_value) && !any_local_work_available) {
        break;
      }

      if (any_lane_needs_refill && !redex_queue_is_empty(global_queue_count_value)) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL_POP;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_SELECT;
      }
#else
      ap_uint<1> any_local_work_available = 0;
      ap_uint<1> any_lane_needs_refill = 0;
      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        ap_uint<1> lane_empty = active_pair_queue_is_local_empty(&reducer_lanes[lane_index]);
        refill_lane_needs_work[lane_index] = lane_empty;
        any_lane_needs_refill = any_lane_needs_refill || lane_empty;
        any_local_work_available = any_local_work_available || !lane_empty;
      }

      if (redex_queue_is_empty(global_queue_count_value) && !any_local_work_available) {
        break;
      }

      if (any_lane_needs_refill && !redex_queue_is_empty(global_queue_count_value)) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL_POP;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_SELECT;
      }
#endif
#else
      ap_uint<1> any_local_work_available = 0;
      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        any_local_work_available =
            any_local_work_available || !active_pair_queue_is_local_empty(&reducer_lanes[lane_index]);
      }
      if (redex_queue_is_empty(global_queue_count_value) && !any_local_work_available) {
        break;
      }

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
      parallel_service_phase = PARALLEL_SERVICE_SELECT;
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_REFILL_POP) {
#if ENABLE_PARALLEL_SPLIT_REFILL
#if ENABLE_PARALLEL_SINGLE_REFILL_POP
      RedexPair refilled_active_pair;
      if (active_pair_queue_pop_global(
              global_active_pair_memory,
              &global_queue_head_index,
              &global_queue_count_value,
              &refilled_active_pair)) {
        if (refill_target_lane == 0) {
          active_pair_queue_push_local(&reducer_lanes[0], refilled_active_pair);
        } else {
          active_pair_queue_push_local(&reducer_lanes[1], refilled_active_pair);
        }
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->global_refill_count = reducer_statistics->global_refill_count + 1;
#endif
      }
      parallel_service_phase = PARALLEL_SERVICE_REFILL;
#else
      RedexPair refilled_active_pair;
      ap_uint<1> popped = 0;
      if (refill_lane_needs_work[0] &&
          active_pair_queue_pop_global(
              global_active_pair_memory,
              &global_queue_head_index,
              &global_queue_count_value,
              &refilled_active_pair)) {
        active_pair_queue_push_local(&reducer_lanes[0], refilled_active_pair);
        refill_lane_needs_work[0] = 0;
        popped = 1;
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->global_refill_count = reducer_statistics->global_refill_count + 1;
#endif
      } else if (refill_lane_needs_work[1] &&
                 active_pair_queue_pop_global(
                     global_active_pair_memory,
                     &global_queue_head_index,
                     &global_queue_count_value,
                     &refilled_active_pair)) {
        active_pair_queue_push_local(&reducer_lanes[1], refilled_active_pair);
        refill_lane_needs_work[1] = 0;
        popped = 1;
#if ENABLE_PARALLEL_REDUCER_STATISTICS
        reducer_statistics->global_refill_count = reducer_statistics->global_refill_count + 1;
#endif
      }

      if (popped && (refill_lane_needs_work[0] || refill_lane_needs_work[1])) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL_POP;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_SELECT;
      }
#endif
#else
      parallel_service_phase = PARALLEL_SERVICE_SELECT;
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_SELECT) {
#if ENABLE_PARALLEL_WAITING_COMMAND
      if (active_command_valid) {
        parallel_service_phase =
            active_command_executed ? PARALLEL_SERVICE_COMMIT : PARALLEL_SERVICE_EXECUTE;
      } else if (waiting_command_valid) {
        active_command_valid = 1;
        active_command_executed = 0;
        active_command_lane_index = waiting_command_lane_index;
        active_command_active_pair = waiting_command_active_pair;
        active_command_rule_kind = waiting_command_rule_kind;
        active_command_produced_pair_index = 0;
        initialize_produced_active_pairs(&active_command_produced_active_pairs);
        waiting_command_valid = 0;
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      } else {
        active_command_valid = 0;
        active_command_executed = 0;
        active_command_lane_index = 0;
        active_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;
        active_command_produced_pair_index = 0;
        initialize_produced_active_pairs(&active_command_produced_active_pairs);
        waiting_command_valid = 0;
        waiting_command_lane_index = 0;
        waiting_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;

        RedexPair lane0_candidate_active_pair;
        RedexPair lane1_candidate_active_pair;
        ap_uint<1> lane0_has_candidate =
            active_pair_queue_peek_local(&reducer_lanes[0], &lane0_candidate_active_pair);
        ap_uint<1> lane1_has_candidate =
            active_pair_queue_peek_local(&reducer_lanes[1], &lane1_candidate_active_pair);

        if (lane0_has_candidate && executed_steps < maximum_steps) {
          active_command_valid = 1;
          active_command_lane_index = 0;
          active_command_active_pair = lane0_candidate_active_pair;
          active_command_rule_kind = get_interaction_rule_kind(
              lane0_candidate_active_pair.left_port,
              lane0_candidate_active_pair.right_port);
        }

        if (lane1_has_candidate &&
            executed_steps + active_command_valid < maximum_steps) {
          InteractionRuleKind lane1_rule_kind = get_interaction_rule_kind(
              lane1_candidate_active_pair.left_port,
              lane1_candidate_active_pair.right_port);
          ap_uint<1> lane1_conflicts =
              active_command_valid &&
              pairwise_active_pairs_conflict(
                  lane1_candidate_active_pair,
                  lane1_rule_kind,
                  active_command_active_pair,
                  active_command_rule_kind,
                  reducer_statistics);
          if (lane1_conflicts) {
            increment_lane_stall_counter(reducer_statistics, 1);
          } else if (active_command_valid) {
            waiting_command_valid = 1;
            waiting_command_lane_index = 1;
            waiting_command_active_pair = lane1_candidate_active_pair;
            waiting_command_rule_kind = lane1_rule_kind;
          } else {
            active_command_valid = 1;
            active_command_lane_index = 1;
            active_command_active_pair = lane1_candidate_active_pair;
            active_command_rule_kind = lane1_rule_kind;
          }
        }

#if ENABLE_PARALLEL_REDUCER_STATISTICS
        if (active_command_valid && waiting_command_valid) {
          reducer_statistics->dual_commit_cycle_count =
              reducer_statistics->dual_commit_cycle_count + 1;
        }
#endif
        if (active_command_valid) {
          parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
        } else {
          parallel_service_phase = PARALLEL_SERVICE_REFILL;
        }
      }
#elif ENABLE_PARALLEL_TWO_COMMAND_BUFFER
      command0_valid = 0;
      command0_lane_index = 0;
      command0_rule_kind = INTERACTION_RULE_UNSUPPORTED;
      command0_produced_pair_index = 0;
      initialize_produced_active_pairs(&command0_produced_active_pairs);

      command1_valid = 0;
      command1_lane_index = 0;
      command1_rule_kind = INTERACTION_RULE_UNSUPPORTED;
      command1_produced_pair_index = 0;
      initialize_produced_active_pairs(&command1_produced_active_pairs);

      RedexPair lane0_candidate_active_pair;
      RedexPair lane1_candidate_active_pair;
      ap_uint<1> lane0_has_candidate =
          active_pair_queue_peek_local(&reducer_lanes[0], &lane0_candidate_active_pair);
      ap_uint<1> lane1_has_candidate =
          active_pair_queue_peek_local(&reducer_lanes[1], &lane1_candidate_active_pair);

      if (lane0_has_candidate && executed_steps < maximum_steps) {
        command0_valid = 1;
        command0_lane_index = 0;
        command0_active_pair = lane0_candidate_active_pair;
        command0_rule_kind = get_interaction_rule_kind(
            lane0_candidate_active_pair.left_port,
            lane0_candidate_active_pair.right_port);
      }

      if (lane1_has_candidate &&
          executed_steps + command0_valid < maximum_steps) {
        InteractionRuleKind lane1_rule_kind = get_interaction_rule_kind(
            lane1_candidate_active_pair.left_port,
            lane1_candidate_active_pair.right_port);
        ap_uint<1> lane1_conflicts =
            command0_valid &&
            pairwise_active_pairs_conflict(
                lane1_candidate_active_pair,
                lane1_rule_kind,
                command0_active_pair,
                command0_rule_kind,
                reducer_statistics);
        if (lane1_conflicts) {
          increment_lane_stall_counter(reducer_statistics, 1);
        } else if (command0_valid) {
          command1_valid = 1;
          command1_lane_index = 1;
          command1_active_pair = lane1_candidate_active_pair;
          command1_rule_kind = lane1_rule_kind;
        } else {
          command0_valid = 1;
          command0_lane_index = 1;
          command0_active_pair = lane1_candidate_active_pair;
          command0_rule_kind = lane1_rule_kind;
        }
      }

#if ENABLE_PARALLEL_REDUCER_STATISTICS
      if (command0_valid && command1_valid) {
        reducer_statistics->dual_commit_cycle_count =
            reducer_statistics->dual_commit_cycle_count + 1;
      }
#endif
      if (command0_valid || command1_valid) {
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      }
#elif ENABLE_PARALLEL_SELECTED_COMMAND
      selected_command_valid = 0;
      selected_command_lane_index = 0;
      selected_command_rule_kind = INTERACTION_RULE_UNSUPPORTED;
      selected_command_produced_pair_index = 0;
      initialize_produced_active_pairs(&selected_command_produced_active_pairs);
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
      selected_command_completion_valid = 0;
#endif
#if ENABLE_PARALLEL_STAGED_LINK
      selected_command_link_active = 0;
      selected_command_link_stage = PARALLEL_LINK_STAGE_IDLE;
#if ENABLE_PARALLEL_LINK_READ_DECODE
      selected_command_link_previous_is_unbound = 0;
#endif
#if ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
      selected_command_link_emit_pair = 0;
#endif
#endif

      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        RedexPair candidate_active_pair;
        ap_uint<1> lane_has_candidate =
            active_pair_queue_peek_local(&reducer_lanes[lane_index], &candidate_active_pair);
        if (!selected_command_valid && lane_has_candidate && executed_steps < maximum_steps) {
          selected_command_valid = 1;
          selected_command_lane_index = lane_index;
          selected_command_active_pair = candidate_active_pair;
#if !ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY
          selected_command_rule_kind = get_interaction_rule_kind(
              candidate_active_pair.left_port,
              candidate_active_pair.right_port);
#endif
        } else if (lane_has_candidate && executed_steps < maximum_steps) {
          increment_lane_stall_counter(reducer_statistics, lane_index);
        }
      }

      if (selected_command_valid) {
#if ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY
        parallel_service_phase = PARALLEL_SERVICE_CLASSIFY;
#else
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
#endif
      } else {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      }
#elif ENABLE_PARALLEL_PAIRWISE_SELECT
      ap_uint<1> lane_has_candidate[REDUCER_LANE_COUNT];
#pragma HLS ARRAY_PARTITION variable=lane_has_candidate complete dim=1

      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        phased_lane_should_commit[lane_index] = 0;
        phased_candidate_rule_kinds[lane_index] = INTERACTION_RULE_UNSUPPORTED;
        initialize_produced_active_pairs(&phased_produced_active_pairs[lane_index]);
        phased_produced_pair_index[lane_index] = 0;
        lane_has_candidate[lane_index] =
            active_pair_queue_peek_local(&reducer_lanes[lane_index], &phased_candidate_active_pairs[lane_index]);
        if (lane_has_candidate[lane_index]) {
          phased_candidate_rule_kinds[lane_index] = get_interaction_rule_kind(
              phased_candidate_active_pairs[lane_index].left_port,
              phased_candidate_active_pairs[lane_index].right_port);
        }
      }

      ap_uint<2> committed_this_cycle_count = 0;
      if (lane_has_candidate[0] && executed_steps < maximum_steps) {
        phased_lane_should_commit[0] = 1;
        committed_this_cycle_count = 1;
      }

      if (lane_has_candidate[1] &&
          executed_steps + committed_this_cycle_count < maximum_steps) {
        ap_uint<1> lane_one_conflicts =
            phased_lane_should_commit[0] &&
            pairwise_active_pairs_conflict(
                phased_candidate_active_pairs[1],
                phased_candidate_rule_kinds[1],
                phased_candidate_active_pairs[0],
                phased_candidate_rule_kinds[0],
                reducer_statistics);
        if (lane_one_conflicts) {
          increment_lane_stall_counter(reducer_statistics, 1);
        } else {
          phased_lane_should_commit[1] = 1;
          committed_this_cycle_count = committed_this_cycle_count + 1;
        }
      }

#if ENABLE_PARALLEL_REDUCER_STATISTICS
      if (committed_this_cycle_count > 1) {
        reducer_statistics->dual_commit_cycle_count =
            reducer_statistics->dual_commit_cycle_count + 1;
      }
#endif
      if (committed_this_cycle_count == 0) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      }
#else
      RedexPair committed_active_pairs[REDUCER_LANE_COUNT];
      InteractionRuleKind committed_rule_kinds[REDUCER_LANE_COUNT];
      ap_uint<32> committed_this_cycle_count = 0;
#pragma HLS ARRAY_PARTITION variable=committed_active_pairs complete dim=1
#pragma HLS ARRAY_PARTITION variable=committed_rule_kinds complete dim=1

      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        phased_lane_should_commit[lane_index] = 0;
        phased_candidate_rule_kinds[lane_index] = INTERACTION_RULE_UNSUPPORTED;
        initialize_produced_active_pairs(&phased_produced_active_pairs[lane_index]);
        phased_produced_pair_index[lane_index] = 0;

        ap_uint<1> lane_has_candidate =
            active_pair_queue_peek_local(&reducer_lanes[lane_index], &phased_candidate_active_pairs[lane_index]);
        if (lane_has_candidate && executed_steps + committed_this_cycle_count < maximum_steps) {
          phased_candidate_rule_kinds[lane_index] = get_interaction_rule_kind(
              phased_candidate_active_pairs[lane_index].left_port,
              phased_candidate_active_pairs[lane_index].right_port);
          ap_uint<1> lane_can_commit_this_cycle =
              !lane_conflicts_with_committed_work(
                  phased_candidate_active_pairs[lane_index],
                  phased_candidate_rule_kinds[lane_index],
                  committed_active_pairs,
                  committed_rule_kinds,
                  committed_this_cycle_count,
                  reducer_statistics);
          if (lane_can_commit_this_cycle) {
            phased_lane_should_commit[lane_index] = 1;
            committed_active_pairs[committed_this_cycle_count] = phased_candidate_active_pairs[lane_index];
            committed_rule_kinds[committed_this_cycle_count] = phased_candidate_rule_kinds[lane_index];
            committed_this_cycle_count = committed_this_cycle_count + 1;
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
      if (committed_this_cycle_count == 0) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      }
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_CLASSIFY) {
#if ENABLE_PARALLEL_SELECTED_COMMAND && ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY
      if (selected_command_valid && local_status_code == REDUCER_STATUS_SUCCESS) {
        selected_command_rule_kind = get_interaction_rule_kind(
            selected_command_active_pair.left_port,
            selected_command_active_pair.right_port);
        parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
      } else {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      }
#else
      parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_STATUS) {
#if ENABLE_PARALLEL_PENDING_STATUS
      if (pending_status_valid) {
        local_status_code = pending_status_code;
        pending_status_valid = 0;
        pending_status_code = REDUCER_STATUS_SUCCESS;
      }
      parallel_service_phase = PARALLEL_SERVICE_REFILL;
#else
      parallel_service_phase = PARALLEL_SERVICE_REFILL;
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_COMPLETE_SELECTED) {
#if ENABLE_PARALLEL_SELECTED_COMMAND && ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
      if (selected_command_completion_valid &&
          local_status_code == REDUCER_STATUS_SUCCESS) {
#if ENABLE_REDUCER_STATISTICS
        count_executed_rule(reducer_statistics, selected_command_completion_rule_kind);
#endif
        executed_steps = executed_steps + 1;
        increment_lane_committed_step_counter(
            reducer_statistics,
            selected_command_completion_lane_index);
        selected_command_completion_valid = 0;
      }
      parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#else
      parallel_service_phase = PARALLEL_SERVICE_REFILL;
#endif
    } else if (parallel_service_phase == PARALLEL_SERVICE_EXECUTE) {
#if ENABLE_PARALLEL_WAITING_COMMAND
      if (active_command_valid &&
          !active_command_executed &&
          local_status_code == REDUCER_STATUS_SUCCESS) {
        ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
            node_memory,
            substitution_memory,
            book_definition_headers,
            book_redex_memory,
            book_node_memory,
            book_definition_count,
            &reducer_lanes[active_command_lane_index],
            allocated_node_count,
            allocated_variable_count,
            active_command_active_pair,
            active_command_rule_kind,
            &active_command_produced_active_pairs,
            reducer_statistics,
            &local_status_code);
        if (lane_committed) {
          executed_steps = executed_steps + 1;
          increment_lane_committed_step_counter(reducer_statistics, active_command_lane_index);
        }
        active_command_executed = 1;
      }
      parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#elif ENABLE_PARALLEL_TWO_COMMAND_BUFFER
      if (command0_valid && local_status_code == REDUCER_STATUS_SUCCESS) {
        ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
            node_memory,
            substitution_memory,
            book_definition_headers,
            book_redex_memory,
            book_node_memory,
            book_definition_count,
            &reducer_lanes[command0_lane_index],
            allocated_node_count,
            allocated_variable_count,
            command0_active_pair,
            command0_rule_kind,
            &command0_produced_active_pairs,
            reducer_statistics,
            &local_status_code);
        if (lane_committed) {
          executed_steps = executed_steps + 1;
          increment_lane_committed_step_counter(reducer_statistics, command0_lane_index);
        }
      }
      if (command1_valid && local_status_code == REDUCER_STATUS_SUCCESS) {
        ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
            node_memory,
            substitution_memory,
            book_definition_headers,
            book_redex_memory,
            book_node_memory,
            book_definition_count,
            &reducer_lanes[command1_lane_index],
            allocated_node_count,
            allocated_variable_count,
            command1_active_pair,
            command1_rule_kind,
            &command1_produced_active_pairs,
            reducer_statistics,
            &local_status_code);
        if (lane_committed) {
          executed_steps = executed_steps + 1;
          increment_lane_committed_step_counter(reducer_statistics, command1_lane_index);
        }
      }
      parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#elif ENABLE_PARALLEL_SELECTED_COMMAND
      if (selected_command_valid && local_status_code == REDUCER_STATUS_SUCCESS) {
#if ENABLE_PARALLEL_STAGED_LINK && ENABLE_RULE_LINK
        if (selected_command_rule_kind == INTERACTION_RULE_LINK) {
          if (!selected_command_link_active) {
            RedexPair discarded_active_pair;
            active_pair_queue_pop_local(
                &reducer_lanes[selected_command_lane_index],
                &discarded_active_pair);

            Port variable_port = selected_command_active_pair.left_port;
            Port other_port = selected_command_active_pair.right_port;

            if (!is_variable_port(variable_port) && is_variable_port(other_port)) {
              variable_port = selected_command_active_pair.right_port;
              other_port = selected_command_active_pair.left_port;
            }

            if (!is_variable_port(variable_port)) {
              if (!append_produced_active_pair(
                      &selected_command_produced_active_pairs,
                      selected_command_active_pair.left_port,
                      selected_command_active_pair.right_port)) {
#if ENABLE_REDUCER_STATISTICS
                reducer_statistics->redex_queue_overflow_count =
                    reducer_statistics->redex_queue_overflow_count + 1;
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
                defer_parallel_status(
                    &pending_status_valid,
                    &pending_status_code,
                    &parallel_service_phase,
                    REDUCER_STATUS_REDEX_QUEUE_OVERFLOW);
#else
                local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
#endif
              } else {
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
                selected_command_completion_valid = 1;
                selected_command_completion_lane_index = selected_command_lane_index;
                selected_command_completion_rule_kind = selected_command_rule_kind;
#else
#if ENABLE_REDUCER_STATISTICS
                count_executed_rule(reducer_statistics, selected_command_rule_kind);
#endif
                executed_steps = executed_steps + 1;
                increment_lane_committed_step_counter(
                    reducer_statistics,
                    selected_command_lane_index);
#endif
              }
#if ENABLE_PARALLEL_PENDING_STATUS
              if (!pending_status_valid) {
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
                if (selected_command_completion_valid) {
                  parallel_service_phase = PARALLEL_SERVICE_COMPLETE_SELECTED;
                } else {
                  parallel_service_phase = PARALLEL_SERVICE_COMMIT;
                }
#else
                parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
              }
#else
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
              if (selected_command_completion_valid) {
                parallel_service_phase = PARALLEL_SERVICE_COMPLETE_SELECTED;
              } else {
                parallel_service_phase = PARALLEL_SERVICE_COMMIT;
              }
#else
              parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
#endif
            } else {
              PortValue variable_index = get_port_value(variable_port);
              if (variable_index >= MAXIMUM_VARIABLE_COUNT) {
#if ENABLE_REDUCER_STATISTICS
                reducer_statistics->unsupported_interaction_count =
                    reducer_statistics->unsupported_interaction_count + 1;
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
                defer_parallel_status(
                    &pending_status_valid,
                    &pending_status_code,
                    &parallel_service_phase,
                    REDUCER_STATUS_UNSUPPORTED_INTERACTION);
#else
                local_status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
                parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
              } else {
                selected_command_link_active = 1;
                selected_command_link_stage = PARALLEL_LINK_STAGE_READ_PREVIOUS;
                selected_command_link_variable_index = variable_index;
                selected_command_link_other_port = other_port;
                parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
              }
            }
          } else if (selected_command_link_stage == PARALLEL_LINK_STAGE_READ_PREVIOUS) {
            Port previous_port =
                substitution_memory[selected_command_link_variable_index];
            selected_command_link_previous_port = previous_port;
#if ENABLE_PARALLEL_LINK_READ_DECODE
            Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
            selected_command_link_previous_is_unbound =
                (previous_port == unbound_marker);
#endif
#if ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT
            selected_command_link_stage = PARALLEL_LINK_STAGE_DECIDE_RESULT;
#else
            selected_command_link_stage = PARALLEL_LINK_STAGE_WRITE_RESULT;
#endif
            parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
#if ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
#if ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT
          } else if (selected_command_link_stage == PARALLEL_LINK_STAGE_DECIDE_RESULT) {
            Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
            selected_command_link_emit_pair =
                (selected_command_link_previous_port == unbound_marker) ? 0 : 1;
            selected_command_link_stage = PARALLEL_LINK_STAGE_WRITE_RESULT;
            parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
#endif
          } else if (selected_command_link_stage == PARALLEL_LINK_STAGE_WRITE_RESULT) {
            Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
#if ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT
            if (selected_command_link_emit_pair) {
              substitution_memory[selected_command_link_variable_index] = unbound_marker;
            } else {
              substitution_memory[selected_command_link_variable_index] =
                  selected_command_link_other_port;
            }
#else
#if ENABLE_PARALLEL_LINK_READ_DECODE
            if (selected_command_link_previous_is_unbound) {
#else
            if (selected_command_link_previous_port == unbound_marker) {
#endif
              substitution_memory[selected_command_link_variable_index] =
                  selected_command_link_other_port;
              selected_command_link_emit_pair = 0;
            } else {
              substitution_memory[selected_command_link_variable_index] = unbound_marker;
              selected_command_link_emit_pair = 1;
            }
#endif
            selected_command_link_stage = PARALLEL_LINK_STAGE_APPEND_RESULT;
            parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
#endif
          } else {
            Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
#if ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
            if (selected_command_link_emit_pair) {
#if ENABLE_PARALLEL_DIRECT_LINK_APPEND && ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER
              selected_command_produced_active_pairs.produced_pairs[0].left_port =
                  selected_command_link_previous_port;
              selected_command_produced_active_pairs.produced_pairs[0].right_port =
                  selected_command_link_other_port;
              selected_command_produced_active_pairs.produced_pair_count = 1;
#else
              if (!append_produced_active_pair(
                      &selected_command_produced_active_pairs,
                      selected_command_link_previous_port,
                      selected_command_link_other_port)) {
#if ENABLE_REDUCER_STATISTICS
                reducer_statistics->redex_queue_overflow_count =
                    reducer_statistics->redex_queue_overflow_count + 1;
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
                defer_parallel_status(
                    &pending_status_valid,
                    &pending_status_code,
                    &parallel_service_phase,
                    REDUCER_STATUS_REDEX_QUEUE_OVERFLOW);
#else
                local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
#endif
              }
#endif
            }
#else
            if (selected_command_link_previous_port == unbound_marker) {
              substitution_memory[selected_command_link_variable_index] =
                  selected_command_link_other_port;
            } else {
              substitution_memory[selected_command_link_variable_index] = unbound_marker;
              if (!append_produced_active_pair(
                      &selected_command_produced_active_pairs,
                      selected_command_link_previous_port,
                      selected_command_link_other_port)) {
#if ENABLE_REDUCER_STATISTICS
                reducer_statistics->redex_queue_overflow_count =
                    reducer_statistics->redex_queue_overflow_count + 1;
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
                defer_parallel_status(
                    &pending_status_valid,
                    &pending_status_code,
                    &parallel_service_phase,
                    REDUCER_STATUS_REDEX_QUEUE_OVERFLOW);
#else
                local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
#endif
              }
            }
#endif

            selected_command_link_active = 0;
            selected_command_link_stage = PARALLEL_LINK_STAGE_IDLE;
#if ENABLE_PARALLEL_LINK_READ_DECODE
            selected_command_link_previous_is_unbound = 0;
#endif
#if ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
            selected_command_link_emit_pair = 0;
#endif
#if ENABLE_PARALLEL_LINK_IMPLICIT_SUCCESS && ENABLE_PARALLEL_DIRECT_LINK_APPEND && ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER && ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
            if (1) {
#else
            if (local_status_code == REDUCER_STATUS_SUCCESS) {
#endif
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
              selected_command_completion_valid = 1;
              selected_command_completion_lane_index = selected_command_lane_index;
              selected_command_completion_rule_kind = selected_command_rule_kind;
#else
#if ENABLE_REDUCER_STATISTICS
              count_executed_rule(reducer_statistics, selected_command_rule_kind);
#endif
              executed_steps = executed_steps + 1;
              increment_lane_committed_step_counter(
                  reducer_statistics,
                  selected_command_lane_index);
#endif
            }
#if ENABLE_PARALLEL_PENDING_STATUS
            if (!pending_status_valid) {
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
              if (selected_command_completion_valid) {
                parallel_service_phase = PARALLEL_SERVICE_COMPLETE_SELECTED;
              } else {
                parallel_service_phase = PARALLEL_SERVICE_COMMIT;
              }
#else
              parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
            }
#else
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
            if (selected_command_completion_valid) {
              parallel_service_phase = PARALLEL_SERVICE_COMPLETE_SELECTED;
            } else {
              parallel_service_phase = PARALLEL_SERVICE_COMMIT;
            }
#else
            parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
#endif
          }
        } else {
#endif
#if ENABLE_PARALLEL_STAGED_LINK
        ap_uint<1> lane_committed = execute_selected_non_link_active_pair_without_queue_commit(
            node_memory,
            substitution_memory,
            book_definition_headers,
            book_redex_memory,
            book_node_memory,
            book_definition_count,
            &reducer_lanes[selected_command_lane_index],
            allocated_node_count,
            allocated_variable_count,
            selected_command_active_pair,
            selected_command_rule_kind,
            &selected_command_produced_active_pairs,
            reducer_statistics,
            &local_status_code);
#else
        ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
            node_memory,
            substitution_memory,
            book_definition_headers,
            book_redex_memory,
            book_node_memory,
            book_definition_count,
            &reducer_lanes[selected_command_lane_index],
            allocated_node_count,
            allocated_variable_count,
            selected_command_active_pair,
            selected_command_rule_kind,
            &selected_command_produced_active_pairs,
            reducer_statistics,
            &local_status_code);
#endif
        if (lane_committed) {
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
          selected_command_completion_valid = 1;
          selected_command_completion_lane_index = selected_command_lane_index;
          selected_command_completion_rule_kind = selected_command_rule_kind;
#else
          executed_steps = executed_steps + 1;
          increment_lane_committed_step_counter(reducer_statistics, selected_command_lane_index);
#endif
        }
#if ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
        if (selected_command_completion_valid) {
          parallel_service_phase = PARALLEL_SERVICE_COMPLETE_SELECTED;
        } else {
          parallel_service_phase = PARALLEL_SERVICE_COMMIT;
        }
#else
        parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
#if ENABLE_PARALLEL_STAGED_LINK && ENABLE_RULE_LINK
        }
#endif
      }
#else
      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        if (phased_lane_should_commit[lane_index] &&
            local_status_code == REDUCER_STATUS_SUCCESS) {
          ap_uint<1> lane_committed = execute_lane_active_pair_without_queue_commit(
              node_memory,
              substitution_memory,
              book_definition_headers,
              book_redex_memory,
              book_node_memory,
              book_definition_count,
              &reducer_lanes[lane_index],
              allocated_node_count,
              allocated_variable_count,
              phased_candidate_active_pairs[lane_index],
              phased_candidate_rule_kinds[lane_index],
              &phased_produced_active_pairs[lane_index],
              reducer_statistics,
              &local_status_code);
          if (lane_committed) {
            executed_steps = executed_steps + 1;
            increment_lane_committed_step_counter(reducer_statistics, lane_index);
          }
        }
      }
      parallel_service_phase = PARALLEL_SERVICE_COMMIT;
#endif
    } else {
#if ENABLE_PARALLEL_WAITING_COMMAND
      ap_uint<1> active_command_has_pending_pair =
          active_command_valid &&
          active_command_produced_pair_index <
              active_command_produced_active_pairs.produced_pair_count;

      if (!active_command_valid || !active_command_has_pending_pair) {
        active_command_valid = 0;
        active_command_executed = 0;
        active_command_produced_pair_index = 0;
        initialize_produced_active_pairs(&active_command_produced_active_pairs);
        if (waiting_command_valid) {
          active_command_valid = 1;
          active_command_lane_index = waiting_command_lane_index;
          active_command_active_pair = waiting_command_active_pair;
          active_command_rule_kind = waiting_command_rule_kind;
          waiting_command_valid = 0;
          parallel_service_phase = PARALLEL_SERVICE_EXECUTE;
        } else {
          parallel_service_phase = PARALLEL_SERVICE_REFILL;
        }
      } else if (local_status_code == REDUCER_STATUS_SUCCESS) {
        if (!commit_one_phased_produced_pair(
                &reducer_lanes[active_command_lane_index],
                global_active_pair_memory,
                &global_queue_tail_index,
                &global_queue_count_value,
                &active_command_produced_active_pairs,
                &active_command_produced_pair_index,
                reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
          reducer_statistics->redex_queue_overflow_count =
              reducer_statistics->redex_queue_overflow_count + 1;
#endif
#if ENABLE_PARALLEL_PENDING_STATUS
          defer_parallel_status(
              &pending_status_valid,
              &pending_status_code,
              &parallel_service_phase,
              REDUCER_STATUS_REDEX_QUEUE_OVERFLOW);
#else
          local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
#endif
        }
      }
#elif ENABLE_PARALLEL_TWO_COMMAND_BUFFER
      ap_uint<1> command0_has_pending_pair =
          command0_valid &&
          command0_produced_pair_index <
              command0_produced_active_pairs.produced_pair_count;
      ap_uint<1> command1_has_pending_pair =
          command1_valid &&
          command1_produced_pair_index <
              command1_produced_active_pairs.produced_pair_count;

      if (!command0_has_pending_pair && !command1_has_pending_pair) {
        command0_valid = 0;
        command1_valid = 0;
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      } else if (command0_has_pending_pair &&
                 local_status_code == REDUCER_STATUS_SUCCESS) {
        if (!commit_one_phased_produced_pair(
                &reducer_lanes[command0_lane_index],
                global_active_pair_memory,
                &global_queue_tail_index,
                &global_queue_count_value,
                &command0_produced_active_pairs,
                &command0_produced_pair_index,
                reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
          reducer_statistics->redex_queue_overflow_count =
              reducer_statistics->redex_queue_overflow_count + 1;
#endif
          local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
        }
      } else if (command1_has_pending_pair &&
                 local_status_code == REDUCER_STATUS_SUCCESS) {
        if (!commit_one_phased_produced_pair(
                &reducer_lanes[command1_lane_index],
                global_active_pair_memory,
                &global_queue_tail_index,
                &global_queue_count_value,
                &command1_produced_active_pairs,
                &command1_produced_pair_index,
                reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
          reducer_statistics->redex_queue_overflow_count =
              reducer_statistics->redex_queue_overflow_count + 1;
#endif
          local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
        }
      }
#elif ENABLE_PARALLEL_SELECTED_COMMAND
      if (!selected_command_valid ||
          selected_command_produced_pair_index >=
              selected_command_produced_active_pairs.produced_pair_count) {
        selected_command_valid = 0;
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      } else {
        if (local_status_code == REDUCER_STATUS_SUCCESS &&
            !commit_one_phased_produced_pair(
                &reducer_lanes[selected_command_lane_index],
                global_active_pair_memory,
                &global_queue_tail_index,
                &global_queue_count_value,
                &selected_command_produced_active_pairs,
                &selected_command_produced_pair_index,
                reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
          reducer_statistics->redex_queue_overflow_count =
              reducer_statistics->redex_queue_overflow_count + 1;
#endif
          local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
        }
      }
#else
      ap_uint<1> any_pending_produced_pair = 0;
      for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
#pragma HLS UNROLL
        if (phased_lane_should_commit[lane_index] &&
            phased_produced_pair_index[lane_index] <
                phased_produced_active_pairs[lane_index].produced_pair_count) {
          any_pending_produced_pair = 1;
        }
      }

      if (!any_pending_produced_pair) {
        parallel_service_phase = PARALLEL_SERVICE_REFILL;
      } else {
        for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
          if (phased_lane_should_commit[lane_index] &&
              phased_produced_pair_index[lane_index] <
                  phased_produced_active_pairs[lane_index].produced_pair_count &&
              local_status_code == REDUCER_STATUS_SUCCESS) {
            if (!commit_one_phased_produced_pair(
                    &reducer_lanes[lane_index],
                    global_active_pair_memory,
                    &global_queue_tail_index,
                    &global_queue_count_value,
                    &phased_produced_active_pairs[lane_index],
                    &phased_produced_pair_index[lane_index],
                    reducer_statistics)) {
#if ENABLE_REDUCER_STATISTICS
              reducer_statistics->redex_queue_overflow_count =
                  reducer_statistics->redex_queue_overflow_count + 1;
#endif
              local_status_code = REDUCER_STATUS_REDEX_QUEUE_OVERFLOW;
            }
            break;
          }
        }
      }
#endif
    }
#endif
#else
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
