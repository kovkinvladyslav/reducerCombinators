#include "reducer_core.hpp"

static inline void record_reducer_failure_status(
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
  } else if (interaction_result.invalid_port_tag) {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->unsupported_interaction_count =
        reducer_statistics->unsupported_interaction_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_INVALID_PORT_TAG;
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
  } else {
#if ENABLE_REDUCER_STATISTICS
    reducer_statistics->unsupported_interaction_count =
        reducer_statistics->unsupported_interaction_count + 1;
#endif
    *local_status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  }
}

static inline void record_reducer_completed_interaction(
    ReducerStatistics* reducer_statistics,
    InteractionRuleKind interaction_rule_kind,
    ap_uint<32>* executed_steps) {
#if ENABLE_REDUCER_STATISTICS
  count_executed_rule(reducer_statistics, interaction_rule_kind);
#endif
  *executed_steps = *executed_steps + 1;
}

static inline void record_reducer_completed_step(ap_uint<32>* executed_steps) {
  *executed_steps = *executed_steps + 1;
}

extern "C" void reducer_core(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* redex_count,
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

  ap_uint<32> redex_queue_head_index = 0;
  ap_uint<32> redex_queue_tail_index = *redex_count;
  ap_uint<32> redex_queue_count_value = *redex_count;
  ap_uint<32> executed_steps = 0;
  ap_uint<32> local_status_code = REDUCER_STATUS_SUCCESS;
#if ENABLE_STAGED_CALL
  CallInstantiationState call_state;
  initialize_call_instantiation_state(&call_state);
#endif
#if ENABLE_PRODUCED_PAIR_BUFFER
  ProducedActivePairs produced_active_pairs;
  initialize_produced_active_pairs(&produced_active_pairs);
  ProducedPairCount produced_pair_flush_index = 0;
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
  ap_uint<1> buffered_rule_is_call = 0;
#else
  InteractionRuleKind buffered_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
#endif
#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_PRODUCED_PAIR_BUFFER
  ReducerServicePhase reducer_service_phase = REDUCER_SERVICE_DISPATCH;
#else
  ap_uint<1> pending_completion_active = 0;
#endif
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
  InteractionRuleKind pending_completion_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
#endif

  while (
#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_PRODUCED_PAIR_BUFFER
      reducer_service_phase != REDUCER_SERVICE_DISPATCH ||
#else
      pending_completion_active ||
#endif
#endif
#if !(ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING && ENABLE_PRODUCED_PAIR_BUFFER)
#if ENABLE_STAGED_CALL
      call_state.active ||
#endif
#if ENABLE_PRODUCED_PAIR_BUFFER
      !produced_active_pairs_are_empty(&produced_active_pairs) ||
#endif
#endif
      (executed_steps < maximum_steps && !redex_queue_is_empty(redex_queue_count_value))) {
#pragma HLS PIPELINE II=1
#if ENABLE_REDUCER_STATISTICS
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_redex_queue_count,
        redex_queue_count_value);
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_allocated_node_count,
        *allocated_node_count);
    update_maximum_observed_counter(
        &reducer_statistics->maximum_observed_allocated_variable_count,
        *allocated_variable_count);
#endif

#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_PRODUCED_PAIR_BUFFER
    if (reducer_service_phase == REDUCER_SERVICE_COMPLETE) {
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
      record_reducer_completed_step(&executed_steps);
#else
      record_reducer_completed_interaction(
          reducer_statistics,
          pending_completion_rule_kind,
          &executed_steps);
#endif
      reducer_service_phase = REDUCER_SERVICE_DISPATCH;
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
      pending_completion_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
      continue;
    }
#else
    if (pending_completion_active) {
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
      record_reducer_completed_step(&executed_steps);
#else
      record_reducer_completed_interaction(
          reducer_statistics,
          pending_completion_rule_kind,
          &executed_steps);
#endif
      pending_completion_active = 0;
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
      pending_completion_rule_kind = INTERACTION_RULE_UNSUPPORTED;
#endif
      continue;
    }
#endif
#endif

    InteractionResult interaction_result = make_interaction_result(1);
    InteractionRuleKind interaction_rule_kind = INTERACTION_RULE_UNSUPPORTED;
    ap_uint<1> interaction_completed = 0;

#if ENABLE_PRODUCED_PAIR_BUFFER
    if (
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
        reducer_service_phase == REDUCER_SERVICE_FLUSH
#else
        !produced_active_pairs_are_empty(&produced_active_pairs)
#endif
    ) {
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
      interaction_rule_kind = buffered_rule_kind;
#endif
      if (!flush_one_produced_active_pair(
              &produced_active_pairs,
              &produced_pair_flush_index,
              redex_memory,
              &redex_queue_tail_index,
              &redex_queue_count_value)) {
        interaction_result.success = 0;
        interaction_result.queue_overflow = 1;
        record_reducer_failure_status(
            reducer_statistics,
            interaction_result,
            &local_status_code);
        initialize_produced_active_pairs(&produced_active_pairs);
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
        reducer_service_phase = REDUCER_SERVICE_DISPATCH;
#endif
        break;
      } else if (produced_pair_flush_index >= produced_active_pairs.produced_pair_count) {
        initialize_produced_active_pairs(&produced_active_pairs);
#if ENABLE_STAGED_CALL
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
        if (buffered_rule_is_call && call_state.active) {
#else
        if (buffered_rule_kind == INTERACTION_RULE_CALL && call_state.active) {
#endif
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
          reducer_service_phase = REDUCER_SERVICE_CALL;
#endif
          continue;
        }
#endif
#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE
        reducer_service_phase = REDUCER_SERVICE_COMPLETE;
#else
        pending_completion_active = 1;
#endif
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
        pending_completion_rule_kind = buffered_rule_kind;
#endif
#else
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
        record_reducer_completed_step(&executed_steps);
#else
        record_reducer_completed_interaction(
            reducer_statistics,
            buffered_rule_kind,
            &executed_steps);
#endif
#endif
        continue;
      }
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
      reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
      continue;
    } else
#endif
#if ENABLE_STAGED_CALL
    if (
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING && ENABLE_PRODUCED_PAIR_BUFFER
        reducer_service_phase == REDUCER_SERVICE_CALL
#else
        call_state.active
#endif
    ) {
      interaction_rule_kind = INTERACTION_RULE_CALL;
#if ENABLE_PRODUCED_PAIR_BUFFER
      initialize_produced_active_pairs(&produced_active_pairs);
      interaction_result = step_staged_call_buffered(
          node_memory,
          substitution_memory,
          &produced_active_pairs,
          book_redex_memory,
          book_node_memory,
          allocated_node_count,
          allocated_variable_count,
          &call_state);
      if (interaction_result.success &&
          !produced_active_pairs_are_empty(&produced_active_pairs)) {
        produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
        reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
        buffered_rule_is_call = 1;
#else
        buffered_rule_kind = interaction_rule_kind;
#endif
        continue;
      }
      if (!interaction_result.success) {
        record_reducer_failure_status(
            reducer_statistics,
            interaction_result,
            &local_status_code);
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
        reducer_service_phase = REDUCER_SERVICE_DISPATCH;
#endif
        break;
      }
      if (call_state.active) {
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
        reducer_service_phase = REDUCER_SERVICE_CALL;
#endif
        continue;
      }
#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE
      reducer_service_phase = REDUCER_SERVICE_COMPLETE;
#else
      pending_completion_active = 1;
#endif
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
      pending_completion_rule_kind = interaction_rule_kind;
#endif
#else
      record_reducer_completed_interaction(
          reducer_statistics,
          interaction_rule_kind,
          &executed_steps);
#endif
      continue;
#else
      interaction_result = step_staged_call(
          node_memory,
          substitution_memory,
          redex_memory,
          book_redex_memory,
          book_node_memory,
          allocated_node_count,
          allocated_variable_count,
          &redex_queue_tail_index,
          &redex_queue_count_value,
          &call_state);
      interaction_completed = !call_state.active;
#endif
    } else
#endif
    {
      RedexPair current_redex_pair;
      if (!redex_queue_pop(redex_memory, &redex_queue_head_index, &redex_queue_count_value, &current_redex_pair)) {
        break;
      }

      Port left_port = current_redex_pair.left_port;
      Port right_port = current_redex_pair.right_port;
      PortTag left_port_tag = get_port_tag(left_port);
      PortTag right_port_tag = get_port_tag(right_port);

      if (!is_supported_port_tag(left_port_tag) || !is_supported_port_tag(right_port_tag)) {
#if ENABLE_REDUCER_STATISTICS
        reducer_statistics->unsupported_interaction_count =
            reducer_statistics->unsupported_interaction_count + 1;
#endif
        local_status_code = REDUCER_STATUS_INVALID_PORT_TAG;
        break;
      }

      interaction_rule_kind = get_interaction_rule_kind(left_port, right_port);
      interaction_completed = 1;
      switch (interaction_rule_kind) {
        case INTERACTION_RULE_LINK:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_link_rule_buffered(
              substitution_memory,
              &produced_active_pairs,
              left_port,
              right_port);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_link_rule(
              substitution_memory,
              redex_memory,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port);
#endif
          break;
        case INTERACTION_RULE_VOID:
          interaction_result = apply_void_rule();
          break;
        case INTERACTION_RULE_ERASE:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_erase_rule_buffered(
              node_memory,
              &produced_active_pairs,
              left_port,
              right_port);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_erase_rule(
              node_memory,
              redex_memory,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port);
#endif
          break;
        case INTERACTION_RULE_ANNIHILATE:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_annihilate_rule_buffered(
              node_memory,
              &produced_active_pairs,
              left_port,
              right_port);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_annihilate_rule(
              node_memory,
              redex_memory,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port);
#endif
          break;
        case INTERACTION_RULE_COMMUTE:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_commute_rule_buffered(
              node_memory,
              substitution_memory,
              &produced_active_pairs,
              allocated_node_count,
              allocated_variable_count,
              redex_queue_count_value,
              left_port,
              right_port);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_commute_rule(
              node_memory,
              substitution_memory,
              redex_memory,
              allocated_node_count,
              allocated_variable_count,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port);
#endif
          break;
#if ENABLE_RULE_OPERATE
        case INTERACTION_RULE_OPERATE:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_operate_rule_buffered(
              node_memory,
              &produced_active_pairs,
              allocated_node_count,
              redex_queue_count_value,
              left_port,
              right_port,
              &local_status_code,
              reducer_statistics);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_operate_rule(
              node_memory,
              redex_memory,
              allocated_node_count,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port,
              &local_status_code,
              reducer_statistics);
#endif
          break;
#endif
#if ENABLE_RULE_SWITCH
        case INTERACTION_RULE_SWITCH:
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = apply_switch_rule_buffered(
              node_memory,
              &produced_active_pairs,
              allocated_node_count,
              redex_queue_count_value,
              left_port,
              right_port,
              &local_status_code);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 0;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
            interaction_completed = 0;
          }
#else
          interaction_result = apply_switch_rule(
              node_memory,
              redex_memory,
              allocated_node_count,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port,
              &local_status_code);
#endif
          break;
#endif
#if ENABLE_RULE_CALL
        case INTERACTION_RULE_CALL:
#if ENABLE_STAGED_CALL
#if ENABLE_PRODUCED_PAIR_BUFFER
          initialize_produced_active_pairs(&produced_active_pairs);
          interaction_result = begin_staged_call_buffered(
              node_memory,
              &produced_active_pairs,
              book_definition_headers,
              book_definition_count,
              *allocated_node_count,
              *allocated_variable_count,
              redex_queue_count_value,
              left_port,
              right_port,
              &call_state);
          if (interaction_result.success &&
              !produced_active_pairs_are_empty(&produced_active_pairs)) {
            produced_pair_flush_index = 0;
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
            reducer_service_phase = REDUCER_SERVICE_FLUSH;
#endif
#if ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS
            buffered_rule_is_call = 1;
#else
            buffered_rule_kind = interaction_rule_kind;
#endif
          }
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING
          if (interaction_result.success &&
              produced_active_pairs_are_empty(&produced_active_pairs) &&
              call_state.active) {
            reducer_service_phase = REDUCER_SERVICE_CALL;
          }
#endif
          interaction_completed =
              !call_state.active && produced_active_pairs_are_empty(&produced_active_pairs);
#else
          interaction_result = begin_staged_call(
              node_memory,
              redex_memory,
              book_definition_headers,
              book_definition_count,
              *allocated_node_count,
              *allocated_variable_count,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port,
              &call_state);
          interaction_completed = !call_state.active;
#endif
#else
          interaction_result = apply_call_rule(
              node_memory,
              substitution_memory,
              redex_memory,
              book_definition_headers,
              book_redex_memory,
              book_node_memory,
              book_definition_count,
              allocated_node_count,
              allocated_variable_count,
              &redex_queue_tail_index,
              &redex_queue_count_value,
              left_port,
              right_port);
#endif
          break;
#endif
        case INTERACTION_RULE_UNSUPPORTED:
        default:
#if ENABLE_REDUCER_STATISTICS
          reducer_statistics->unsupported_interaction_count =
              reducer_statistics->unsupported_interaction_count + 1;
#endif
          local_status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
          break;
      }
    }
    if (local_status_code != REDUCER_STATUS_SUCCESS && interaction_result.success) {
      break;
    }

    if (!interaction_completed && interaction_result.success) {
      continue;
    }

    if (!interaction_result.success) {
      record_reducer_failure_status(
          reducer_statistics,
          interaction_result,
          &local_status_code);
      break;
    }

#if ENABLE_DEFERRED_COMPLETION_ACCOUNTING
#if ENABLE_COMPACT_SERVICE_PHASE && ENABLE_PRODUCED_PAIR_BUFFER
    reducer_service_phase = REDUCER_SERVICE_COMPLETE;
#else
    pending_completion_active = 1;
#endif
#if !(ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS)
    pending_completion_rule_kind = interaction_rule_kind;
#endif
    continue;
#else
    record_reducer_completed_interaction(
        reducer_statistics,
        interaction_rule_kind,
        &executed_steps);
#endif
  }

  if (local_status_code == REDUCER_STATUS_SUCCESS &&
      executed_steps == maximum_steps &&
      !redex_queue_is_empty(redex_queue_count_value)) {
    local_status_code = REDUCER_STATUS_MAXIMUM_STEPS_REACHED;
  }

  *redex_count = redex_queue_count(redex_queue_count_value);
  *performed_steps = executed_steps;
  *status_code = local_status_code;
#if ENABLE_REDUCER_STATISTICS
  reducer_statistics->performed_step_count = executed_steps;
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_redex_queue_count,
      redex_queue_count_value);
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_allocated_node_count,
      *allocated_node_count);
  update_maximum_observed_counter(
      &reducer_statistics->maximum_observed_allocated_variable_count,
      *allocated_variable_count);
#endif
}
