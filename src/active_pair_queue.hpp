#ifndef ACTIVE_PAIR_QUEUE_HPP
#define ACTIVE_PAIR_QUEUE_HPP

#include "redex_queue.hpp"

static inline void initialize_reducer_lane_state(ReducerLaneState* reducer_lane_state) {
  reducer_lane_state->local_queue_head_index = 0;
  reducer_lane_state->local_queue_tail_index = 0;
  reducer_lane_state->local_queue_count = 0;
  reducer_lane_state->lane_is_idle = 1;
}

static inline ap_uint<1> active_pair_queue_is_local_empty(ReducerLaneState* reducer_lane_state) {
  return reducer_lane_state->local_queue_count == 0;
}

static inline ap_uint<1> active_pair_queue_is_local_full(ReducerLaneState* reducer_lane_state) {
  return reducer_lane_state->local_queue_count >= LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH;
}

static inline ap_uint<1> active_pair_queue_push_local(
    ReducerLaneState* reducer_lane_state,
    RedexPair active_pair_to_push) {
  if (active_pair_queue_is_local_full(reducer_lane_state)) {
    return 0;
  }

  reducer_lane_state->local_active_pair_queue[reducer_lane_state->local_queue_tail_index] =
      active_pair_to_push;
  reducer_lane_state->local_queue_tail_index =
      (reducer_lane_state->local_queue_tail_index + 1) % LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH;
  reducer_lane_state->local_queue_count = reducer_lane_state->local_queue_count + 1;
  reducer_lane_state->lane_is_idle = 0;
  return 1;
}

static inline ap_uint<1> active_pair_queue_pop_local(
    ReducerLaneState* reducer_lane_state,
    RedexPair* popped_active_pair) {
  if (active_pair_queue_is_local_empty(reducer_lane_state)) {
    reducer_lane_state->lane_is_idle = 1;
    return 0;
  }

  *popped_active_pair =
      reducer_lane_state->local_active_pair_queue[reducer_lane_state->local_queue_head_index];
  reducer_lane_state->local_queue_head_index =
      (reducer_lane_state->local_queue_head_index + 1) % LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH;
  reducer_lane_state->local_queue_count = reducer_lane_state->local_queue_count - 1;
  reducer_lane_state->lane_is_idle = reducer_lane_state->local_queue_count == 0;
  return 1;
}

static inline ap_uint<1> active_pair_queue_peek_local(
    ReducerLaneState* reducer_lane_state,
    RedexPair* peeked_active_pair) {
  if (active_pair_queue_is_local_empty(reducer_lane_state)) {
    return 0;
  }

  *peeked_active_pair =
      reducer_lane_state->local_active_pair_queue[reducer_lane_state->local_queue_head_index];
  return 1;
}

static inline ap_uint<1> active_pair_queue_push_global(
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value,
    RedexPair active_pair_to_push) {
  return redex_queue_push(
      global_active_pair_memory,
      global_queue_tail_index,
      global_queue_count_value,
      active_pair_to_push);
}

static inline ap_uint<1> active_pair_queue_pop_global(
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_head_index,
    ap_uint<32>* global_queue_count_value,
    RedexPair* popped_active_pair) {
  return redex_queue_pop(
      global_active_pair_memory,
      global_queue_head_index,
      global_queue_count_value,
      popped_active_pair);
}

static inline void initialize_produced_active_pairs(ProducedActivePairs* produced_active_pairs) {
  produced_active_pairs->produced_pair_count = 0;
}

static inline ap_uint<1> produced_active_pairs_are_empty(
    ProducedActivePairs* produced_active_pairs) {
  return produced_active_pairs->produced_pair_count == 0;
}

static inline ap_uint<1> append_produced_active_pair(
    ProducedActivePairs* produced_active_pairs,
    Port left_port,
    Port right_port) {
  if (produced_active_pairs->produced_pair_count >= MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION) {
    return 0;
  }

  ap_uint<32> produced_pair_index = produced_active_pairs->produced_pair_count;
  produced_active_pairs->produced_pairs[produced_pair_index].left_port = left_port;
  produced_active_pairs->produced_pairs[produced_pair_index].right_port = right_port;
  produced_active_pairs->produced_pair_count = produced_active_pairs->produced_pair_count + 1;
  return 1;
}

static inline ap_uint<1> flush_one_produced_active_pair(
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* produced_pair_flush_index,
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* global_queue_tail_index,
    ap_uint<32>* global_queue_count_value) {
  if (*produced_pair_flush_index >= produced_active_pairs->produced_pair_count) {
    return 1;
  }

  RedexPair active_pair_to_flush =
      produced_active_pairs->produced_pairs[*produced_pair_flush_index];
  if (!active_pair_queue_push_global(
          global_active_pair_memory,
          global_queue_tail_index,
          global_queue_count_value,
          active_pair_to_flush)) {
    return 0;
  }

  *produced_pair_flush_index = *produced_pair_flush_index + 1;
  return 1;
}

#endif
