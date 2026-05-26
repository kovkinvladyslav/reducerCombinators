#ifndef INTERACTION_RULES_HPP
#define INTERACTION_RULES_HPP

#include "book_storage.hpp"
#include "active_pair_queue.hpp"
#include "numeric_encoding.hpp"
#include "node_storage.hpp"
#include "redex_queue.hpp"
#include "reducer_statistics.hpp"

static inline InteractionResult make_interaction_result(ap_uint<1> success) {
  InteractionResult result;
  result.success = success;
  result.unsupported_interaction = 0;
  result.queue_overflow = 0;
  result.node_memory_exhausted = 0;
  result.variable_memory_exhausted = 0;
  result.invalid_port_tag = 0;
  result.invalid_numeric_operation = 0;
  result.numeric_division_by_zero = 0;
  result.unsupported_numeric_type = 0;
  result.unknown_reference = 0;
  result.book_node_overflow = 0;
  result.book_redex_overflow = 0;
  return result;
}

static inline InteractionResult apply_void_rule() {
  return make_interaction_result(1);
}

static inline ap_uint<1> push_reduction_redex(
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  RedexPair redex_pair;
  redex_pair.left_port = left_port;
  redex_pair.right_port = right_port;
  return redex_queue_push(redex_memory, redex_queue_tail_index, redex_queue_count_value, redex_pair);
}

static inline ap_uint<1> append_reduction_redex(
    ProducedActivePairs* produced_active_pairs,
    Port left_port,
    Port right_port) {
  return append_produced_active_pair(produced_active_pairs, left_port, right_port);
}

static inline Port apply_numeric_operation(
    Port active_numeric_port,
    Port captured_numeric_operation_port,
    ap_uint<32>* status_code,
    ReducerStatistics* reducer_statistics) {
  NumericTag operation_tag = get_numeric_tag_from_port(captured_numeric_operation_port);
  NumericTag active_numeric_tag = get_numeric_tag_from_port(active_numeric_port);

  if (!is_supported_numeric_operation_tag(operation_tag)) {
    *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
    return make_u24_number_port(0);
  }
  if (!ENABLE_NUMERIC_DIVISION &&
      (operation_tag == NUMERIC_TAG_OPERATION_DIVIDE ||
       operation_tag == NUMERIC_TAG_OPERATION_FLIPPED_DIVIDE)) {
    *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
    return make_u24_number_port(0);
  }
  if (!ENABLE_NUMERIC_REMAINDER &&
      (operation_tag == NUMERIC_TAG_OPERATION_REMAINDER ||
       operation_tag == NUMERIC_TAG_OPERATION_FLIPPED_REMAINDER)) {
    *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
    return make_u24_number_port(0);
  }
  if (!is_numeric_type_tag(active_numeric_tag)) {
    *status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
    return make_u24_number_port(0);
  }
  if (!ENABLE_NUMERIC_F24 && active_numeric_tag == NUMERIC_TAG_F24) {
    *status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
    return make_u24_number_port(0);
  }

  ap_uint<24> captured_operand_u24 = get_numeric_payload_from_port(captured_numeric_operation_port);
  ap_uint<24> active_operand_u24 = get_u24_from_numeric_port(active_numeric_port);
  ap_int<24> captured_operand_i24 = ap_int<24>(captured_operand_u24);
  ap_int<24> active_operand_i24 = get_i24_from_numeric_port(active_numeric_port);
#if ENABLE_NUMERIC_F24
  float captured_operand_f24 = get_f24_from_numeric_port(captured_numeric_operation_port);
  float active_operand_f24 = get_f24_from_numeric_port(active_numeric_port);
#endif

  if (active_numeric_tag == NUMERIC_TAG_U24) {
    ap_uint<24> result_u24 = 0;
    switch (operation_tag) {
      case NUMERIC_TAG_OPERATION_ADD: result_u24 = captured_operand_u24 + active_operand_u24; break;
      case NUMERIC_TAG_OPERATION_SUBTRACT: result_u24 = captured_operand_u24 - active_operand_u24; break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SUBTRACT: result_u24 = active_operand_u24 - captured_operand_u24; break;
      case NUMERIC_TAG_OPERATION_MULTIPLY: result_u24 = captured_operand_u24 * active_operand_u24; break;
#if ENABLE_NUMERIC_DIVISION
      case NUMERIC_TAG_OPERATION_DIVIDE:
        if (active_operand_u24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_u24 = captured_operand_u24 / active_operand_u24;
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_DIVIDE:
        if (captured_operand_u24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_u24 = active_operand_u24 / captured_operand_u24;
        break;
#endif
#if ENABLE_NUMERIC_REMAINDER
      case NUMERIC_TAG_OPERATION_REMAINDER:
        if (active_operand_u24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_u24 = captured_operand_u24 % active_operand_u24;
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_REMAINDER:
        if (captured_operand_u24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_u24 = active_operand_u24 % captured_operand_u24;
        break;
#endif
      case NUMERIC_TAG_OPERATION_EQUAL: result_u24 = (captured_operand_u24 == active_operand_u24); break;
      case NUMERIC_TAG_OPERATION_NOT_EQUAL: result_u24 = (captured_operand_u24 != active_operand_u24); break;
      case NUMERIC_TAG_OPERATION_LESS_THAN: result_u24 = (captured_operand_u24 < active_operand_u24); break;
      case NUMERIC_TAG_OPERATION_GREATER_THAN: result_u24 = (captured_operand_u24 > active_operand_u24); break;
      case NUMERIC_TAG_OPERATION_AND: result_u24 = captured_operand_u24 & active_operand_u24; break;
      case NUMERIC_TAG_OPERATION_OR: result_u24 = captured_operand_u24 | active_operand_u24; break;
      case NUMERIC_TAG_OPERATION_XOR: result_u24 = captured_operand_u24 ^ active_operand_u24; break;
      case NUMERIC_TAG_OPERATION_SHIFT_LEFT: result_u24 = captured_operand_u24 << (active_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_LEFT: result_u24 = active_operand_u24 << (captured_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_SHIFT_RIGHT: result_u24 = captured_operand_u24 >> (active_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_RIGHT: result_u24 = active_operand_u24 >> (captured_operand_u24 & 31); break;
      default:
        *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
        return make_u24_number_port(0);
    }
    return make_u24_number_port(result_u24);
  }

  if (active_numeric_tag == NUMERIC_TAG_I24) {
    ap_int<24> result_i24 = 0;
    switch (operation_tag) {
      case NUMERIC_TAG_OPERATION_ADD: result_i24 = captured_operand_i24 + active_operand_i24; break;
      case NUMERIC_TAG_OPERATION_SUBTRACT: result_i24 = captured_operand_i24 - active_operand_i24; break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SUBTRACT: result_i24 = active_operand_i24 - captured_operand_i24; break;
      case NUMERIC_TAG_OPERATION_MULTIPLY: result_i24 = captured_operand_i24 * active_operand_i24; break;
#if ENABLE_NUMERIC_DIVISION
      case NUMERIC_TAG_OPERATION_DIVIDE:
        if (active_operand_i24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_i24 = captured_operand_i24 / active_operand_i24;
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_DIVIDE:
        if (captured_operand_i24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_i24 = active_operand_i24 / captured_operand_i24;
        break;
#endif
#if ENABLE_NUMERIC_REMAINDER
      case NUMERIC_TAG_OPERATION_REMAINDER:
        if (active_operand_i24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_i24 = captured_operand_i24 % active_operand_i24;
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_REMAINDER:
        if (captured_operand_i24 == 0) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_i24 = active_operand_i24 % captured_operand_i24;
        break;
#endif
      case NUMERIC_TAG_OPERATION_EQUAL: return make_u24_number_port(captured_operand_i24 == active_operand_i24);
      case NUMERIC_TAG_OPERATION_NOT_EQUAL: return make_u24_number_port(captured_operand_i24 != active_operand_i24);
      case NUMERIC_TAG_OPERATION_LESS_THAN: return make_u24_number_port(captured_operand_i24 < active_operand_i24);
      case NUMERIC_TAG_OPERATION_GREATER_THAN: return make_u24_number_port(captured_operand_i24 > active_operand_i24);
      case NUMERIC_TAG_OPERATION_AND: result_i24 = captured_operand_i24 & active_operand_i24; break;
      case NUMERIC_TAG_OPERATION_OR: result_i24 = captured_operand_i24 | active_operand_i24; break;
      case NUMERIC_TAG_OPERATION_XOR: result_i24 = captured_operand_i24 ^ active_operand_i24; break;
      case NUMERIC_TAG_OPERATION_SHIFT_LEFT: result_i24 = captured_operand_i24 << (active_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_LEFT: result_i24 = active_operand_i24 << (captured_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_SHIFT_RIGHT: result_i24 = captured_operand_i24 >> (active_operand_u24 & 31); break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SHIFT_RIGHT: result_i24 = active_operand_i24 >> (captured_operand_u24 & 31); break;
      default:
        *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
        return make_u24_number_port(0);
    }
    return make_i24_number_port(result_i24);
  }

#if ENABLE_NUMERIC_F24
  if (active_numeric_tag == NUMERIC_TAG_F24) {
    float result_f24 = 0.0f;
    switch (operation_tag) {
      case NUMERIC_TAG_OPERATION_ADD: result_f24 = captured_operand_f24 + active_operand_f24; break;
      case NUMERIC_TAG_OPERATION_SUBTRACT: result_f24 = captured_operand_f24 - active_operand_f24; break;
      case NUMERIC_TAG_OPERATION_FLIPPED_SUBTRACT: result_f24 = active_operand_f24 - captured_operand_f24; break;
      case NUMERIC_TAG_OPERATION_MULTIPLY: result_f24 = captured_operand_f24 * active_operand_f24; break;
#if ENABLE_NUMERIC_DIVISION
      case NUMERIC_TAG_OPERATION_DIVIDE:
        if (active_operand_f24 == 0.0f) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_f24 = captured_operand_f24 / active_operand_f24;
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_DIVIDE:
        if (captured_operand_f24 == 0.0f) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_f24 = active_operand_f24 / captured_operand_f24;
        break;
#endif
#if ENABLE_NUMERIC_REMAINDER
      case NUMERIC_TAG_OPERATION_REMAINDER:
        if (active_operand_f24 == 0.0f) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_f24 = std::fmod(captured_operand_f24, active_operand_f24);
        break;
      case NUMERIC_TAG_OPERATION_FLIPPED_REMAINDER:
        if (captured_operand_f24 == 0.0f) {
          *status_code = REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO;
          return make_u24_number_port(0);
        }
        result_f24 = std::fmod(active_operand_f24, captured_operand_f24);
        break;
#endif
      case NUMERIC_TAG_OPERATION_EQUAL: return make_u24_number_port(captured_operand_f24 == active_operand_f24);
      case NUMERIC_TAG_OPERATION_NOT_EQUAL: return make_u24_number_port(captured_operand_f24 != active_operand_f24);
      case NUMERIC_TAG_OPERATION_LESS_THAN: return make_u24_number_port(captured_operand_f24 < active_operand_f24);
      case NUMERIC_TAG_OPERATION_GREATER_THAN: return make_u24_number_port(captured_operand_f24 > active_operand_f24);
      default:
        *status_code = REDUCER_STATUS_INVALID_NUMERIC_OPERATION;
        return make_u24_number_port(0);
    }
    return make_f24_number_port(result_f24);
  }
#endif

  if (reducer_statistics != 0) {
    reducer_statistics->invalid_numeric_operation_count = reducer_statistics->invalid_numeric_operation_count + 1;
  }
  *status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
  return make_u24_number_port(0);
}

static inline InteractionResult apply_link_rule(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  Port variable_port = left_port;
  Port other_port = right_port;

  if (!is_variable_port(variable_port) && is_variable_port(other_port)) {
    variable_port = right_port;
    other_port = left_port;
  }

  if (!is_variable_port(variable_port)) {
    if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, left_port, right_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  PortValue variable_index = get_port_value(variable_port);
  if (variable_index >= MAXIMUM_VARIABLE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  Port previous_port = substitution_memory[variable_index];
  Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  if (previous_port == unbound_marker) {
    substitution_memory[variable_index] = other_port;
    return result;
  }

  substitution_memory[variable_index] = unbound_marker;
  if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, previous_port, other_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_link_rule_buffered(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  Port variable_port = left_port;
  Port other_port = right_port;

  if (!is_variable_port(variable_port) && is_variable_port(other_port)) {
    variable_port = right_port;
    other_port = left_port;
  }

  if (!is_variable_port(variable_port)) {
    if (!append_produced_active_pair(produced_active_pairs, left_port, right_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  PortValue variable_index = get_port_value(variable_port);
  if (variable_index >= MAXIMUM_VARIABLE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  Port previous_port = substitution_memory[variable_index];
  Port unbound_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  if (previous_port == unbound_marker) {
    substitution_memory[variable_index] = other_port;
    return result;
  }

  substitution_memory[variable_index] = unbound_marker;
  if (!append_produced_active_pair(produced_active_pairs, previous_port, other_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_erase_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  Port erasure_port = left_port;
  Port node_port = right_port;
  if (!is_erasure_port(erasure_port)) {
    erasure_port = right_port;
    node_port = left_port;
  }

  NodeAddress node_address = get_port_value(node_port);
  if (node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode node_data = node_memory[node_address];
  Port first_auxiliary_port = get_first_auxiliary_port(node_data);
  Port second_auxiliary_port = get_second_auxiliary_port(node_data);

  if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, erasure_port, first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, erasure_port, second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_erase_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  Port erasure_port = left_port;
  Port node_port = right_port;
  if (!is_erasure_port(erasure_port)) {
    erasure_port = right_port;
    node_port = left_port;
  }

  NodeAddress node_address = get_port_value(node_port);
  if (node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode node_data = node_memory[node_address];
  Port first_auxiliary_port = get_first_auxiliary_port(node_data);
  Port second_auxiliary_port = get_second_auxiliary_port(node_data);

  if (!append_produced_active_pair(produced_active_pairs, erasure_port, first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!append_produced_active_pair(produced_active_pairs, erasure_port, second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_annihilate_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  NodeAddress left_node_address = get_port_value(left_port);
  NodeAddress right_node_address = get_port_value(right_port);
  if (left_node_address >= MAXIMUM_NODE_COUNT || right_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode left_node = node_memory[left_node_address];
  PackedNode right_node = node_memory[right_node_address];
  Port left_first_auxiliary_port = get_first_auxiliary_port(left_node);
  Port left_second_auxiliary_port = get_second_auxiliary_port(left_node);
  Port right_first_auxiliary_port = get_first_auxiliary_port(right_node);
  Port right_second_auxiliary_port = get_second_auxiliary_port(right_node);

  if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, left_first_auxiliary_port, right_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!push_reduction_redex(redex_memory, redex_queue_tail_index, redex_queue_count_value, left_second_auxiliary_port, right_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_annihilate_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  NodeAddress left_node_address = get_port_value(left_port);
  NodeAddress right_node_address = get_port_value(right_port);
  if (left_node_address >= MAXIMUM_NODE_COUNT || right_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode left_node = node_memory[left_node_address];
  PackedNode right_node = node_memory[right_node_address];
  Port left_first_auxiliary_port = get_first_auxiliary_port(left_node);
  Port left_second_auxiliary_port = get_second_auxiliary_port(left_node);
  Port right_first_auxiliary_port = get_first_auxiliary_port(right_node);
  Port right_second_auxiliary_port = get_second_auxiliary_port(right_node);

  if (!append_produced_active_pair(
          produced_active_pairs,
          left_first_auxiliary_port,
          right_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!append_produced_active_pair(
          produced_active_pairs,
          left_second_auxiliary_port,
          right_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_operate_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port,
    ap_uint<32>* status_code,
    ReducerStatistics* reducer_statistics) {
  InteractionResult result = make_interaction_result(1);
  Port numeric_active_port = left_port;
  Port operator_node_port = right_port;
  if (get_port_tag(left_port) != PORT_TAG_NUMBER) {
    numeric_active_port = right_port;
    operator_node_port = left_port;
  }

  NodeAddress operator_node_address = get_port_value(operator_node_port);
  if (operator_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode operator_node = node_memory[operator_node_address];
  Port operator_first_auxiliary_port = get_first_auxiliary_port(operator_node);
  Port operator_second_auxiliary_port = get_second_auxiliary_port(operator_node);

  if (get_port_tag(operator_first_auxiliary_port) == PORT_TAG_NUMBER) {
    Port produced_numeric_result_port = apply_numeric_operation(
        numeric_active_port,
        operator_first_auxiliary_port,
        status_code,
        reducer_statistics);
    if (*status_code == REDUCER_STATUS_INVALID_NUMERIC_OPERATION) {
      result.success = 0;
      result.invalid_numeric_operation = 1;
      return result;
    }
    if (*status_code == REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO) {
      result.success = 0;
      result.numeric_division_by_zero = 1;
      return result;
    }
    if (*status_code == REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE) {
      result.success = 0;
      result.unsupported_numeric_type = 1;
      return result;
    }
    if (!push_reduction_redex(
            redex_memory,
            redex_queue_tail_index,
            redex_queue_count_value,
            produced_numeric_result_port,
            operator_second_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  if (*allocated_node_count >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (*redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  ap_uint<1> allocation_successful = 0;
  NodeAddress delayed_operator_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      numeric_active_port,
      operator_second_auxiliary_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }

  Port delayed_operator_node_port = make_port(PORT_TAG_OPERATOR, delayed_operator_node_address);
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          operator_first_auxiliary_port,
          delayed_operator_node_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_operate_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
    ap_uint<32> redex_queue_count_value,
    Port left_port,
    Port right_port,
    ap_uint<32>* status_code,
    ReducerStatistics* reducer_statistics) {
  InteractionResult result = make_interaction_result(1);
  Port numeric_active_port = left_port;
  Port operator_node_port = right_port;
  if (get_port_tag(left_port) != PORT_TAG_NUMBER) {
    numeric_active_port = right_port;
    operator_node_port = left_port;
  }

  NodeAddress operator_node_address = get_port_value(operator_node_port);
  if (operator_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode operator_node = node_memory[operator_node_address];
  Port operator_first_auxiliary_port = get_first_auxiliary_port(operator_node);
  Port operator_second_auxiliary_port = get_second_auxiliary_port(operator_node);

  if (get_port_tag(operator_first_auxiliary_port) == PORT_TAG_NUMBER) {
    Port produced_numeric_result_port = apply_numeric_operation(
        numeric_active_port,
        operator_first_auxiliary_port,
        status_code,
        reducer_statistics);
    if (*status_code == REDUCER_STATUS_INVALID_NUMERIC_OPERATION) {
      result.success = 0;
      result.invalid_numeric_operation = 1;
      return result;
    }
    if (*status_code == REDUCER_STATUS_NUMERIC_DIVISION_BY_ZERO) {
      result.success = 0;
      result.numeric_division_by_zero = 1;
      return result;
    }
    if (*status_code == REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE) {
      result.success = 0;
      result.unsupported_numeric_type = 1;
      return result;
    }
    if (!append_reduction_redex(
            produced_active_pairs,
            produced_numeric_result_port,
            operator_second_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  if (*allocated_node_count >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  ap_uint<1> allocation_successful = 0;
  NodeAddress delayed_operator_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      numeric_active_port,
      operator_second_auxiliary_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }

  Port delayed_operator_node_port = make_port(PORT_TAG_OPERATOR, delayed_operator_node_address);
  if (!append_reduction_redex(
          produced_active_pairs,
          operator_first_auxiliary_port,
          delayed_operator_node_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_switch_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port,
    ap_uint<32>* status_code) {
  InteractionResult result = make_interaction_result(1);
  Port numeric_active_port = left_port;
  Port switch_node_port = right_port;
  if (get_port_tag(left_port) != PORT_TAG_NUMBER) {
    numeric_active_port = right_port;
    switch_node_port = left_port;
  }

  NodeAddress switch_node_address = get_port_value(switch_node_port);
  if (switch_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode switch_node = node_memory[switch_node_address];
  Port switch_first_auxiliary_port = get_first_auxiliary_port(switch_node);
  Port switch_second_auxiliary_port = get_second_auxiliary_port(switch_node);
  NumericTag numeric_tag = get_numeric_tag_from_port(numeric_active_port);

  ap_uint<1> is_zero_value = 0;
  if (numeric_tag == NUMERIC_TAG_U24) {
    is_zero_value = get_u24_from_numeric_port(numeric_active_port) == 0;
  } else if (numeric_tag == NUMERIC_TAG_I24) {
    is_zero_value = get_i24_from_numeric_port(numeric_active_port) == 0;
#if ENABLE_NUMERIC_F24
  } else if (numeric_tag == NUMERIC_TAG_F24) {
    is_zero_value = get_f24_from_numeric_port(numeric_active_port) == 0.0f;
#endif
  } else {
    *status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
    result.success = 0;
    result.unsupported_numeric_type = 1;
    return result;
  }

  Port erasure_port = make_port(PORT_TAG_ERASURE, 0);
  ap_uint<1> allocation_successful = 0;
  if (is_zero_value) {
    if (*allocated_node_count >= MAXIMUM_NODE_COUNT || *redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
      result.success = 0;
      result.node_memory_exhausted = (*allocated_node_count >= MAXIMUM_NODE_COUNT);
      result.queue_overflow = (*redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH);
      return result;
    }
    NodeAddress zero_branch_constructor_node_address = allocate_node(
        node_memory,
        allocated_node_count,
        switch_second_auxiliary_port,
        erasure_port,
        &allocation_successful);
    if (!allocation_successful) {
      result.success = 0;
      result.node_memory_exhausted = 1;
      return result;
    }
    Port zero_branch_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, zero_branch_constructor_node_address);
    if (!push_reduction_redex(
            redex_memory,
            redex_queue_tail_index,
            redex_queue_count_value,
            zero_branch_constructor_node_port,
            switch_first_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  if (*allocated_node_count > (MAXIMUM_NODE_COUNT - 2) || *redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.node_memory_exhausted = (*allocated_node_count > (MAXIMUM_NODE_COUNT - 2));
    result.queue_overflow = (*redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH);
    return result;
  }

  Port decremented_numeric_port = numeric_active_port;
  if (numeric_tag == NUMERIC_TAG_U24) {
    decremented_numeric_port = make_u24_number_port(get_u24_from_numeric_port(numeric_active_port) - 1);
  } else if (numeric_tag == NUMERIC_TAG_I24) {
    decremented_numeric_port = make_i24_number_port(get_i24_from_numeric_port(numeric_active_port) - 1);
#if ENABLE_NUMERIC_F24
  } else if (numeric_tag == NUMERIC_TAG_F24) {
    decremented_numeric_port = make_f24_number_port(get_f24_from_numeric_port(numeric_active_port) - 1.0f);
#endif
  }

  NodeAddress inner_constructor_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      decremented_numeric_port,
      switch_second_auxiliary_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  Port inner_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, inner_constructor_node_address);
  NodeAddress outer_constructor_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      erasure_port,
      inner_constructor_node_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  Port outer_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, outer_constructor_node_address);
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          outer_constructor_node_port,
          switch_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_switch_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
    ap_uint<32> redex_queue_count_value,
    Port left_port,
    Port right_port,
    ap_uint<32>* status_code) {
  InteractionResult result = make_interaction_result(1);
  Port numeric_active_port = left_port;
  Port switch_node_port = right_port;
  if (get_port_tag(left_port) != PORT_TAG_NUMBER) {
    numeric_active_port = right_port;
    switch_node_port = left_port;
  }

  NodeAddress switch_node_address = get_port_value(switch_node_port);
  if (switch_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode switch_node = node_memory[switch_node_address];
  Port switch_first_auxiliary_port = get_first_auxiliary_port(switch_node);
  Port switch_second_auxiliary_port = get_second_auxiliary_port(switch_node);
  NumericTag numeric_tag = get_numeric_tag_from_port(numeric_active_port);

  ap_uint<1> is_zero_value = 0;
  if (numeric_tag == NUMERIC_TAG_U24) {
    is_zero_value = get_u24_from_numeric_port(numeric_active_port) == 0;
  } else if (numeric_tag == NUMERIC_TAG_I24) {
    is_zero_value = get_i24_from_numeric_port(numeric_active_port) == 0;
#if ENABLE_NUMERIC_F24
  } else if (numeric_tag == NUMERIC_TAG_F24) {
    is_zero_value = get_f24_from_numeric_port(numeric_active_port) == 0.0f;
#endif
  } else {
    *status_code = REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE;
    result.success = 0;
    result.unsupported_numeric_type = 1;
    return result;
  }

  Port erasure_port = make_port(PORT_TAG_ERASURE, 0);
  ap_uint<1> allocation_successful = 0;
  if (is_zero_value) {
    if (*allocated_node_count >= MAXIMUM_NODE_COUNT || redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
      result.success = 0;
      result.node_memory_exhausted = (*allocated_node_count >= MAXIMUM_NODE_COUNT);
      result.queue_overflow = (redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH);
      return result;
    }
    NodeAddress zero_branch_constructor_node_address = allocate_node(
        node_memory,
        allocated_node_count,
        switch_second_auxiliary_port,
        erasure_port,
        &allocation_successful);
    if (!allocation_successful) {
      result.success = 0;
      result.node_memory_exhausted = 1;
      return result;
    }
    Port zero_branch_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, zero_branch_constructor_node_address);
    if (!append_reduction_redex(
            produced_active_pairs,
            zero_branch_constructor_node_port,
            switch_first_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
    return result;
  }

  if (*allocated_node_count > (MAXIMUM_NODE_COUNT - 2) || redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.node_memory_exhausted = (*allocated_node_count > (MAXIMUM_NODE_COUNT - 2));
    result.queue_overflow = (redex_queue_count_value >= MAXIMUM_REDEX_QUEUE_LENGTH);
    return result;
  }

  Port decremented_numeric_port = numeric_active_port;
  if (numeric_tag == NUMERIC_TAG_U24) {
    decremented_numeric_port = make_u24_number_port(get_u24_from_numeric_port(numeric_active_port) - 1);
  } else if (numeric_tag == NUMERIC_TAG_I24) {
    decremented_numeric_port = make_i24_number_port(get_i24_from_numeric_port(numeric_active_port) - 1);
#if ENABLE_NUMERIC_F24
  } else if (numeric_tag == NUMERIC_TAG_F24) {
    decremented_numeric_port = make_f24_number_port(get_f24_from_numeric_port(numeric_active_port) - 1.0f);
#endif
  }

  NodeAddress inner_constructor_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      decremented_numeric_port,
      switch_second_auxiliary_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  Port inner_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, inner_constructor_node_address);
  NodeAddress outer_constructor_node_address = allocate_node(
      node_memory,
      allocated_node_count,
      erasure_port,
      inner_constructor_node_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  Port outer_constructor_node_port = make_port(PORT_TAG_CONSTRUCTOR, outer_constructor_node_address);
  if (!append_reduction_redex(
          produced_active_pairs,
          outer_constructor_node_port,
          switch_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_commute_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);

  NodeAddress left_node_address = get_port_value(left_port);
  NodeAddress right_node_address = get_port_value(right_port);
  if (left_node_address >= MAXIMUM_NODE_COUNT || right_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PortTag left_binary_node_tag = get_port_tag(left_port);
  PortTag right_binary_node_tag = get_port_tag(right_port);

  PackedNode left_node = node_memory[left_node_address];
  PackedNode right_node = node_memory[right_node_address];
  Port left_first_auxiliary_port = get_first_auxiliary_port(left_node);
  Port left_second_auxiliary_port = get_second_auxiliary_port(left_node);
  Port right_first_auxiliary_port = get_first_auxiliary_port(right_node);
  Port right_second_auxiliary_port = get_second_auxiliary_port(right_node);

  if (*allocated_node_count > (MAXIMUM_NODE_COUNT - 4)) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (*allocated_variable_count > (MAXIMUM_ALLOCATABLE_VARIABLE_COUNT - 4)) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }
  if (*redex_queue_count_value > (MAXIMUM_REDEX_QUEUE_LENGTH - 4)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  Port fresh_variable_x_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 0));
  Port fresh_variable_y_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 1));
  Port fresh_variable_z_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 2));
  Port fresh_variable_w_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 3));
  Port unbound_variable_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  substitution_memory[*allocated_variable_count + 0] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 1] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 2] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 3] = unbound_variable_marker;
  *allocated_variable_count = *allocated_variable_count + 4;

  ap_uint<1> allocation_successful = 0;
  NodeAddress node_ry_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_x_port,
      fresh_variable_y_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_zw_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_z_port,
      fresh_variable_w_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_xz_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_x_port,
      fresh_variable_z_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_yw_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_y_port,
      fresh_variable_w_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }

  Port right_node_instance_for_left_first_auxiliary_port =
      make_port(right_binary_node_tag, node_ry_address);
  Port right_node_instance_for_left_second_auxiliary_port =
      make_port(right_binary_node_tag, node_zw_address);
  Port left_node_instance_for_right_first_auxiliary_port =
      make_port(left_binary_node_tag, node_xz_address);
  Port left_node_instance_for_right_second_auxiliary_port =
      make_port(left_binary_node_tag, node_yw_address);

  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          right_node_instance_for_left_first_auxiliary_port,
          left_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          right_node_instance_for_left_second_auxiliary_port,
          left_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          left_node_instance_for_right_first_auxiliary_port,
          right_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          left_node_instance_for_right_second_auxiliary_port,
          right_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_commute_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);

  NodeAddress left_node_address = get_port_value(left_port);
  NodeAddress right_node_address = get_port_value(right_port);
  if (left_node_address >= MAXIMUM_NODE_COUNT || right_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PortTag left_binary_node_tag = get_port_tag(left_port);
  PortTag right_binary_node_tag = get_port_tag(right_port);

  PackedNode left_node = node_memory[left_node_address];
  PackedNode right_node = node_memory[right_node_address];
  Port left_first_auxiliary_port = get_first_auxiliary_port(left_node);
  Port left_second_auxiliary_port = get_second_auxiliary_port(left_node);
  Port right_first_auxiliary_port = get_first_auxiliary_port(right_node);
  Port right_second_auxiliary_port = get_second_auxiliary_port(right_node);

  if (*allocated_node_count > (MAXIMUM_NODE_COUNT - 4)) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (*allocated_variable_count > (MAXIMUM_ALLOCATABLE_VARIABLE_COUNT - 4)) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }
  if (redex_queue_count_value > (MAXIMUM_REDEX_QUEUE_LENGTH - 4)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  Port fresh_variable_x_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 0));
  Port fresh_variable_y_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 1));
  Port fresh_variable_z_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 2));
  Port fresh_variable_w_port = make_port(PORT_TAG_VARIABLE, PortValue(*allocated_variable_count + 3));
  Port unbound_variable_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  substitution_memory[*allocated_variable_count + 0] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 1] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 2] = unbound_variable_marker;
  substitution_memory[*allocated_variable_count + 3] = unbound_variable_marker;
  *allocated_variable_count = *allocated_variable_count + 4;

  ap_uint<1> allocation_successful = 0;
  NodeAddress node_ry_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_x_port,
      fresh_variable_y_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_zw_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_z_port,
      fresh_variable_w_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_xz_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_x_port,
      fresh_variable_z_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_yw_address = allocate_node(
      node_memory,
      allocated_node_count,
      fresh_variable_y_port,
      fresh_variable_w_port,
      &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }

  Port right_node_instance_for_left_first_auxiliary_port =
      make_port(right_binary_node_tag, node_ry_address);
  Port right_node_instance_for_left_second_auxiliary_port =
      make_port(right_binary_node_tag, node_zw_address);
  Port left_node_instance_for_right_first_auxiliary_port =
      make_port(left_binary_node_tag, node_xz_address);
  Port left_node_instance_for_right_second_auxiliary_port =
      make_port(left_binary_node_tag, node_yw_address);

  if (!append_reduction_redex(
          produced_active_pairs,
          right_node_instance_for_left_first_auxiliary_port,
          left_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!append_reduction_redex(
          produced_active_pairs,
          right_node_instance_for_left_second_auxiliary_port,
          left_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!append_reduction_redex(
          produced_active_pairs,
          left_node_instance_for_right_first_auxiliary_port,
          right_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }
  if (!append_reduction_redex(
          produced_active_pairs,
          left_node_instance_for_right_second_auxiliary_port,
          right_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_safe_reference_duplication_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port reference_port,
    Port duplicator_port) {
  InteractionResult result = make_interaction_result(1);
  NodeAddress duplicator_node_address = get_port_value(duplicator_port);
  if (duplicator_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }
  if (*redex_queue_count_value > (MAXIMUM_REDEX_QUEUE_LENGTH - 2)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  PackedNode duplicator_node = node_memory[duplicator_node_address];
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          reference_port,
          get_first_auxiliary_port(duplicator_node)) ||
      !push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          reference_port,
          get_second_auxiliary_port(duplicator_node))) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_safe_reference_duplication_rule_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32> redex_queue_count_value,
    Port reference_port,
    Port duplicator_port) {
  InteractionResult result = make_interaction_result(1);
  NodeAddress duplicator_node_address = get_port_value(duplicator_port);
  if (duplicator_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }
  if (redex_queue_count_value > (MAXIMUM_REDEX_QUEUE_LENGTH - 2)) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  PackedNode duplicator_node = node_memory[duplicator_node_address];
  if (!append_reduction_redex(
          produced_active_pairs,
          reference_port,
          get_first_auxiliary_port(duplicator_node)) ||
      !append_reduction_redex(
          produced_active_pairs,
          reference_port,
          get_second_auxiliary_port(duplicator_node))) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_call_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port) {
  InteractionResult result = make_interaction_result(1);
  Port reference_port = left_port;
  Port call_argument_port = right_port;
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    reference_port = right_port;
    call_argument_port = left_port;
  }
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PortValue reference_identifier = get_port_value(reference_port);
  if (reference_identifier >= book_definition_count ||
      reference_identifier >= MAXIMUM_BOOK_DEFINITION_COUNT) {
    result.success = 0;
    result.unknown_reference = 1;
    return result;
  }

  BookDefinitionHeader definition_header = book_definition_headers[reference_identifier];
  if (definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION ||
      definition_header.node_base_index + definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT) {
    result.success = 0;
    result.book_node_overflow = 1;
    return result;
  }
  if (definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION ||
      definition_header.redex_base_index + definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT) {
    result.success = 0;
    result.book_redex_overflow = 1;
    return result;
  }
  if (definition_header.variable_count > MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }

  if (definition_header.definition_is_safe &&
      get_port_tag(call_argument_port) == PORT_TAG_DUPLICATOR) {
    return apply_safe_reference_duplication_rule(
        node_memory,
        redex_memory,
        redex_queue_tail_index,
        redex_queue_count_value,
        reference_port,
        call_argument_port);
  }

  if (*allocated_node_count + definition_header.node_count > MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (*allocated_variable_count + definition_header.variable_count > MAXIMUM_ALLOCATABLE_VARIABLE_COUNT) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }
  if (*redex_queue_count_value + definition_header.redex_count + 1 > MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  ap_uint<32> fresh_node_base_index = *allocated_node_count;
  ap_uint<32> fresh_variable_base_index = *allocated_variable_count;
  Port unbound_variable_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);

  for (int variable_index = 0; variable_index < MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION; ++variable_index) {
    if (variable_index < definition_header.variable_count) {
      substitution_memory[fresh_variable_base_index + variable_index] = unbound_variable_marker;
    }
  }
  *allocated_variable_count = *allocated_variable_count + definition_header.variable_count;

  ap_uint<1> adjustment_successful = 1;
  for (int node_index = 0; node_index < MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION; ++node_index) {
    if (node_index < definition_header.node_count) {
      PackedNode definition_node = book_node_memory[definition_header.node_base_index + node_index];
      Port adjusted_first_auxiliary_port = adjust_book_port(
          get_first_auxiliary_port(definition_node),
          fresh_node_base_index,
          fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      Port adjusted_second_auxiliary_port = adjust_book_port(
          get_second_auxiliary_port(definition_node),
          fresh_node_base_index,
          fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful) {
        result.success = 0;
        result.unsupported_interaction = 1;
        return result;
      }
      ap_uint<1> allocation_successful = 0;
      allocate_node(
          node_memory,
          allocated_node_count,
          adjusted_first_auxiliary_port,
          adjusted_second_auxiliary_port,
          &allocation_successful);
      if (!allocation_successful) {
        result.success = 0;
        result.node_memory_exhausted = 1;
        return result;
      }
    }
  }

  for (int redex_index = 0; redex_index < MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION; ++redex_index) {
    if (redex_index < definition_header.redex_count) {
      RedexPair adjusted_definition_redex = adjust_book_redex(
          book_redex_memory[definition_header.redex_base_index + redex_index],
          fresh_node_base_index,
          fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful ||
          !push_reduction_redex(
              redex_memory,
              redex_queue_tail_index,
              redex_queue_count_value,
              adjusted_definition_redex.left_port,
              adjusted_definition_redex.right_port)) {
        result.success = 0;
        result.queue_overflow = !adjustment_successful ? ap_uint<1>(0) : ap_uint<1>(1);
        result.unsupported_interaction = !adjustment_successful;
        return result;
      }
    }
  }

  Port adjusted_root_port = adjust_book_port(
      definition_header.root_port,
      fresh_node_base_index,
      fresh_variable_base_index,
      definition_header.node_count,
      definition_header.variable_count,
      &adjustment_successful);
  if (!adjustment_successful) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }
  if (!push_reduction_redex(
          redex_memory,
          redex_queue_tail_index,
          redex_queue_count_value,
          adjusted_root_port,
          call_argument_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline void initialize_call_instantiation_state(CallInstantiationState* call_state) {
  call_state->active = 0;
  call_state->phase = CALL_STAGE_IDLE;
  call_state->reference_port = make_port(PORT_TAG_ERASURE, 0);
  call_state->call_argument_port = make_port(PORT_TAG_ERASURE, 0);
  call_state->definition_header = make_empty_book_definition_header();
  call_state->fresh_node_base_index = 0;
  call_state->fresh_variable_base_index = 0;
  call_state->variable_index = 0;
  call_state->node_index = 0;
  call_state->redex_index = 0;
}

static inline InteractionResult begin_staged_call(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32> allocated_node_count,
    ap_uint<32> allocated_variable_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    Port left_port,
    Port right_port,
    CallInstantiationState* call_state) {
  InteractionResult result = make_interaction_result(1);
  Port reference_port = left_port;
  Port call_argument_port = right_port;
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    reference_port = right_port;
    call_argument_port = left_port;
  }
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PortValue reference_identifier = get_port_value(reference_port);
  if (reference_identifier >= book_definition_count ||
      reference_identifier >= MAXIMUM_BOOK_DEFINITION_COUNT) {
    result.success = 0;
    result.unknown_reference = 1;
    return result;
  }

  BookDefinitionHeader definition_header = book_definition_headers[reference_identifier];
  if (definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION ||
      definition_header.node_base_index + definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT) {
    result.success = 0;
    result.book_node_overflow = 1;
    return result;
  }
  if (definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION ||
      definition_header.redex_base_index + definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT) {
    result.success = 0;
    result.book_redex_overflow = 1;
    return result;
  }
  if (definition_header.variable_count > MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }

  if (definition_header.definition_is_safe &&
      get_port_tag(call_argument_port) == PORT_TAG_DUPLICATOR) {
    return apply_safe_reference_duplication_rule(
        node_memory,
        redex_memory,
        redex_queue_tail_index,
        redex_queue_count_value,
        reference_port,
        call_argument_port);
  }

  if (allocated_node_count + definition_header.node_count > MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (allocated_variable_count + definition_header.variable_count > MAXIMUM_ALLOCATABLE_VARIABLE_COUNT) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }
  if (*redex_queue_count_value + definition_header.redex_count + 1 > MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  call_state->active = 1;
  call_state->phase = CALL_STAGE_INITIALIZE_VARIABLES;
  call_state->reference_port = reference_port;
  call_state->call_argument_port = call_argument_port;
  call_state->definition_header = definition_header;
  call_state->fresh_node_base_index = allocated_node_count;
  call_state->fresh_variable_base_index = allocated_variable_count;
  call_state->variable_index = 0;
  call_state->node_index = 0;
  call_state->redex_index = 0;
  return result;
}

static inline InteractionResult begin_staged_call_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32> allocated_node_count,
    ap_uint<32> allocated_variable_count,
    ap_uint<32> redex_queue_count_value,
    Port left_port,
    Port right_port,
    CallInstantiationState* call_state) {
  InteractionResult result = make_interaction_result(1);
  Port reference_port = left_port;
  Port call_argument_port = right_port;
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    reference_port = right_port;
    call_argument_port = left_port;
  }
  if (get_port_tag(reference_port) != PORT_TAG_REFERENCE) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PortValue reference_identifier = get_port_value(reference_port);
  if (reference_identifier >= book_definition_count ||
      reference_identifier >= MAXIMUM_BOOK_DEFINITION_COUNT) {
    result.success = 0;
    result.unknown_reference = 1;
    return result;
  }

  BookDefinitionHeader definition_header = book_definition_headers[reference_identifier];
  if (definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION ||
      definition_header.node_base_index + definition_header.node_count > MAXIMUM_BOOK_NODE_COUNT) {
    result.success = 0;
    result.book_node_overflow = 1;
    return result;
  }
  if (definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION ||
      definition_header.redex_base_index + definition_header.redex_count > MAXIMUM_BOOK_REDEX_COUNT) {
    result.success = 0;
    result.book_redex_overflow = 1;
    return result;
  }
  if (definition_header.variable_count > MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }

  if (definition_header.definition_is_safe &&
      get_port_tag(call_argument_port) == PORT_TAG_DUPLICATOR) {
    return apply_safe_reference_duplication_rule_buffered(
        node_memory,
        produced_active_pairs,
        redex_queue_count_value,
        reference_port,
        call_argument_port);
  }

  if (allocated_node_count + definition_header.node_count > MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  if (allocated_variable_count + definition_header.variable_count > MAXIMUM_ALLOCATABLE_VARIABLE_COUNT) {
    result.success = 0;
    result.variable_memory_exhausted = 1;
    return result;
  }
  if (redex_queue_count_value + definition_header.redex_count + 1 > MAXIMUM_REDEX_QUEUE_LENGTH) {
    result.success = 0;
    result.queue_overflow = 1;
    return result;
  }

  call_state->active = 1;
  call_state->phase = CALL_STAGE_INITIALIZE_VARIABLES;
  call_state->reference_port = reference_port;
  call_state->call_argument_port = call_argument_port;
  call_state->definition_header = definition_header;
  call_state->fresh_node_base_index = allocated_node_count;
  call_state->fresh_variable_base_index = allocated_variable_count;
  call_state->variable_index = 0;
  call_state->node_index = 0;
  call_state->redex_index = 0;
  return result;
}

static inline InteractionResult step_staged_call(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32>* redex_queue_tail_index,
    ap_uint<32>* redex_queue_count_value,
    CallInstantiationState* call_state) {
  InteractionResult result = make_interaction_result(1);
  BookDefinitionHeader definition_header = call_state->definition_header;
  Port unbound_variable_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  ap_uint<1> adjustment_successful = 1;

  if (!call_state->active) {
    return result;
  }

  if (call_state->phase == CALL_STAGE_INITIALIZE_VARIABLES) {
    if (call_state->variable_index < definition_header.variable_count) {
      substitution_memory[call_state->fresh_variable_base_index + call_state->variable_index] =
          unbound_variable_marker;
      call_state->variable_index = call_state->variable_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_COPY_NODES;
    return result;
  }

  if (call_state->phase == CALL_STAGE_COPY_NODES) {
    if (call_state->node_index < definition_header.node_count) {
      PackedNode definition_node = book_node_memory[definition_header.node_base_index + call_state->node_index];
      Port adjusted_first_auxiliary_port = adjust_book_port(
          get_first_auxiliary_port(definition_node),
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      Port adjusted_second_auxiliary_port = adjust_book_port(
          get_second_auxiliary_port(definition_node),
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.unsupported_interaction = 1;
        return result;
      }
      node_memory[call_state->fresh_node_base_index + call_state->node_index] =
          make_packed_node(adjusted_first_auxiliary_port, adjusted_second_auxiliary_port);
      call_state->node_index = call_state->node_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_PUSH_REDEXES;
    return result;
  }

  if (call_state->phase == CALL_STAGE_PUSH_REDEXES) {
    if (call_state->redex_index < definition_header.redex_count) {
      RedexPair adjusted_definition_redex = adjust_book_redex(
          book_redex_memory[definition_header.redex_base_index + call_state->redex_index],
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.unsupported_interaction = 1;
        return result;
      }
      if (!push_reduction_redex(
              redex_memory,
              redex_queue_tail_index,
              redex_queue_count_value,
              adjusted_definition_redex.left_port,
              adjusted_definition_redex.right_port)) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.queue_overflow = 1;
        return result;
      }
      call_state->redex_index = call_state->redex_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_PUSH_ROOT;
    return result;
  }

  if (call_state->phase == CALL_STAGE_PUSH_ROOT) {
    Port adjusted_root_port = adjust_book_port(
        definition_header.root_port,
        call_state->fresh_node_base_index,
        call_state->fresh_variable_base_index,
        definition_header.node_count,
        definition_header.variable_count,
        &adjustment_successful);
    if (!adjustment_successful) {
      call_state->active = 0;
      call_state->phase = CALL_STAGE_IDLE;
      result.success = 0;
      result.unsupported_interaction = 1;
      return result;
    }
    if (!push_reduction_redex(
            redex_memory,
            redex_queue_tail_index,
            redex_queue_count_value,
            adjusted_root_port,
            call_state->call_argument_port)) {
      call_state->active = 0;
      call_state->phase = CALL_STAGE_IDLE;
      result.success = 0;
      result.queue_overflow = 1;
      return result;
    }
    *allocated_node_count = *allocated_node_count + definition_header.node_count;
    *allocated_variable_count = *allocated_variable_count + definition_header.variable_count;
    call_state->active = 0;
    call_state->phase = CALL_STAGE_IDLE;
    return result;
  }

  call_state->active = 0;
  call_state->phase = CALL_STAGE_IDLE;
  result.success = 0;
  result.unsupported_interaction = 1;
  return result;
}

static inline InteractionResult step_staged_call_buffered(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    CallInstantiationState* call_state) {
  InteractionResult result = make_interaction_result(1);
  BookDefinitionHeader definition_header = call_state->definition_header;
  Port unbound_variable_marker = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  ap_uint<1> adjustment_successful = 1;

  if (!call_state->active) {
    return result;
  }

  if (call_state->phase == CALL_STAGE_INITIALIZE_VARIABLES) {
    if (call_state->variable_index < definition_header.variable_count) {
      substitution_memory[call_state->fresh_variable_base_index + call_state->variable_index] =
          unbound_variable_marker;
      call_state->variable_index = call_state->variable_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_COPY_NODES;
    return result;
  }

  if (call_state->phase == CALL_STAGE_COPY_NODES) {
    if (call_state->node_index < definition_header.node_count) {
      PackedNode definition_node = book_node_memory[definition_header.node_base_index + call_state->node_index];
      Port adjusted_first_auxiliary_port = adjust_book_port(
          get_first_auxiliary_port(definition_node),
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      Port adjusted_second_auxiliary_port = adjust_book_port(
          get_second_auxiliary_port(definition_node),
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.unsupported_interaction = 1;
        return result;
      }
      node_memory[call_state->fresh_node_base_index + call_state->node_index] =
          make_packed_node(adjusted_first_auxiliary_port, adjusted_second_auxiliary_port);
      call_state->node_index = call_state->node_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_PUSH_REDEXES;
    return result;
  }

  if (call_state->phase == CALL_STAGE_PUSH_REDEXES) {
    if (call_state->redex_index < definition_header.redex_count) {
      RedexPair adjusted_definition_redex = adjust_book_redex(
          book_redex_memory[definition_header.redex_base_index + call_state->redex_index],
          call_state->fresh_node_base_index,
          call_state->fresh_variable_base_index,
          definition_header.node_count,
          definition_header.variable_count,
          &adjustment_successful);
      if (!adjustment_successful) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.unsupported_interaction = 1;
        return result;
      }
      if (!append_reduction_redex(
              produced_active_pairs,
              adjusted_definition_redex.left_port,
              adjusted_definition_redex.right_port)) {
        call_state->active = 0;
        call_state->phase = CALL_STAGE_IDLE;
        result.success = 0;
        result.queue_overflow = 1;
        return result;
      }
      call_state->redex_index = call_state->redex_index + 1;
      return result;
    }
    call_state->phase = CALL_STAGE_PUSH_ROOT;
    return result;
  }

  if (call_state->phase == CALL_STAGE_PUSH_ROOT) {
    Port adjusted_root_port = adjust_book_port(
        definition_header.root_port,
        call_state->fresh_node_base_index,
        call_state->fresh_variable_base_index,
        definition_header.node_count,
        definition_header.variable_count,
        &adjustment_successful);
    if (!adjustment_successful) {
      call_state->active = 0;
      call_state->phase = CALL_STAGE_IDLE;
      result.success = 0;
      result.unsupported_interaction = 1;
      return result;
    }
    if (!append_reduction_redex(
            produced_active_pairs,
            adjusted_root_port,
            call_state->call_argument_port)) {
      call_state->active = 0;
      call_state->phase = CALL_STAGE_IDLE;
      result.success = 0;
      result.queue_overflow = 1;
      return result;
    }
    *allocated_node_count = *allocated_node_count + definition_header.node_count;
    *allocated_variable_count = *allocated_variable_count + definition_header.variable_count;
    call_state->active = 0;
    call_state->phase = CALL_STAGE_IDLE;
    return result;
  }

  call_state->active = 0;
  call_state->phase = CALL_STAGE_IDLE;
  result.success = 0;
  result.unsupported_interaction = 1;
  return result;
}

#endif
