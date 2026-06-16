#ifndef REDEX_QUEUE_HPP
#define REDEX_QUEUE_HPP

#include "reducer_types.hpp"

static inline ap_uint<1> redex_queue_is_empty(ap_uint<32> redex_queue_count) {
  return redex_queue_count == 0;
}

static inline ap_uint<1> redex_queue_is_full(ap_uint<32> redex_queue_count) {
  return redex_queue_count >= MAXIMUM_REDEX_QUEUE_LENGTH;
}

static inline ap_uint<32> redex_queue_count(ap_uint<32> redex_queue_count_value) {
  return redex_queue_count_value;
}

static inline ap_uint<1> redex_queue_push(
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    RedexPair redex_pair_to_push) {
  if (redex_queue_is_full(*redex_queue_count_value)) {
    return 0;
  }

  redex_memory[*redex_queue_tail_index] = redex_pair_to_push;
  *redex_queue_tail_index = (*redex_queue_tail_index + 1) % MAXIMUM_REDEX_QUEUE_LENGTH;
  *redex_queue_count_value = *redex_queue_count_value + 1;
  return 1;
}

static inline ap_uint<1> redex_queue_pop(
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_head_index,
    ap_uint<32>* redex_queue_count_value,
    RedexPair* popped_redex_pair) {
  if (redex_queue_is_empty(*redex_queue_count_value)) {
    return 0;
  }

  *popped_redex_pair = redex_memory[*redex_queue_head_index];
  *redex_queue_head_index = (*redex_queue_head_index + 1) % MAXIMUM_REDEX_QUEUE_LENGTH;
  *redex_queue_count_value = *redex_queue_count_value - 1;
  return 1;
}

#endif
