#ifndef REDUCER_STATISTICS_HPP
#define REDUCER_STATISTICS_HPP

#include "interaction_rule_kind.hpp"

struct ReducerStatistics {
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_step_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_link_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_void_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_erase_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_annihilate_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_commute_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_operate_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_switch_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> performed_call_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> unsupported_interaction_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> unknown_reference_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> node_allocation_failure_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> redex_queue_overflow_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> variable_memory_overflow_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> invalid_numeric_operation_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> numeric_division_by_zero_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> maximum_observed_redex_queue_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> maximum_observed_allocated_node_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> maximum_observed_allocated_variable_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> lane_committed_step_count[REDUCER_LANE_COUNT];
  ap_uint<REDUCER_COUNTER_BIT_COUNT> lane_stall_count[REDUCER_LANE_COUNT];
  ap_uint<REDUCER_COUNTER_BIT_COUNT> parallel_cycle_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> dual_commit_cycle_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> global_refill_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> local_queue_spill_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> global_queue_contention_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> allocation_contention_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> substitution_conflict_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> node_access_conflict_count;
  ap_uint<REDUCER_COUNTER_BIT_COUNT> maximum_observed_lane_queue_count[REDUCER_LANE_COUNT];
  ap_uint<REDUCER_COUNTER_BIT_COUNT> maximum_observed_global_queue_count;
};

static inline void clear_reducer_statistics(ReducerStatistics* reducer_statistics) {
  reducer_statistics->performed_step_count = 0;
  reducer_statistics->performed_link_count = 0;
  reducer_statistics->performed_void_count = 0;
  reducer_statistics->performed_erase_count = 0;
  reducer_statistics->performed_annihilate_count = 0;
  reducer_statistics->performed_commute_count = 0;
  reducer_statistics->performed_operate_count = 0;
  reducer_statistics->performed_switch_count = 0;
  reducer_statistics->performed_call_count = 0;
  reducer_statistics->unsupported_interaction_count = 0;
  reducer_statistics->unknown_reference_count = 0;
  reducer_statistics->node_allocation_failure_count = 0;
  reducer_statistics->redex_queue_overflow_count = 0;
  reducer_statistics->variable_memory_overflow_count = 0;
  reducer_statistics->invalid_numeric_operation_count = 0;
  reducer_statistics->numeric_division_by_zero_count = 0;
  reducer_statistics->maximum_observed_redex_queue_count = 0;
  reducer_statistics->maximum_observed_allocated_node_count = 0;
  reducer_statistics->maximum_observed_allocated_variable_count = 0;
  for (int lane_index = 0; lane_index < REDUCER_LANE_COUNT; ++lane_index) {
    reducer_statistics->lane_committed_step_count[lane_index] = 0;
    reducer_statistics->lane_stall_count[lane_index] = 0;
    reducer_statistics->maximum_observed_lane_queue_count[lane_index] = 0;
  }
  reducer_statistics->parallel_cycle_count = 0;
  reducer_statistics->dual_commit_cycle_count = 0;
  reducer_statistics->global_refill_count = 0;
  reducer_statistics->local_queue_spill_count = 0;
  reducer_statistics->global_queue_contention_count = 0;
  reducer_statistics->allocation_contention_count = 0;
  reducer_statistics->substitution_conflict_count = 0;
  reducer_statistics->node_access_conflict_count = 0;
  reducer_statistics->maximum_observed_global_queue_count = 0;
}

static inline void update_maximum_observed_counter(
    ap_uint<REDUCER_COUNTER_BIT_COUNT>* maximum_counter,
    ap_uint<32> observed_value) {
  if (observed_value > *maximum_counter) {
    *maximum_counter = observed_value;
  }
}

static inline void count_executed_rule(
    ReducerStatistics* reducer_statistics,
    InteractionRuleKind interaction_rule_kind) {
  switch (interaction_rule_kind) {
    case INTERACTION_RULE_LINK:
      reducer_statistics->performed_link_count = reducer_statistics->performed_link_count + 1;
      break;
    case INTERACTION_RULE_VOID:
      reducer_statistics->performed_void_count = reducer_statistics->performed_void_count + 1;
      break;
    case INTERACTION_RULE_ERASE:
      reducer_statistics->performed_erase_count = reducer_statistics->performed_erase_count + 1;
      break;
    case INTERACTION_RULE_ANNIHILATE:
      reducer_statistics->performed_annihilate_count = reducer_statistics->performed_annihilate_count + 1;
      break;
    case INTERACTION_RULE_COMMUTE:
      reducer_statistics->performed_commute_count = reducer_statistics->performed_commute_count + 1;
      break;
    case INTERACTION_RULE_OPERATE:
      reducer_statistics->performed_operate_count = reducer_statistics->performed_operate_count + 1;
      break;
    case INTERACTION_RULE_SWITCH:
      reducer_statistics->performed_switch_count = reducer_statistics->performed_switch_count + 1;
      break;
    case INTERACTION_RULE_CALL:
      reducer_statistics->performed_call_count = reducer_statistics->performed_call_count + 1;
      break;
    case INTERACTION_RULE_UNSUPPORTED:
    default:
      break;
  }
}

#endif
