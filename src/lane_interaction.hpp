#ifndef LANE_INTERACTION_HPP
#define LANE_INTERACTION_HPP

#include "active_pair_queue.hpp"
#include "interaction_dispatch.hpp"
#include "interaction_rules.hpp"

static inline ap_uint<1> produced_rule_needs_allocation(InteractionRuleKind interaction_rule_kind) {
  ap_uint<1> needs_allocation = 0;
#if ENABLE_RULE_COMMUTE
  needs_allocation = needs_allocation || interaction_rule_kind == INTERACTION_RULE_COMMUTE;
#endif
#if ENABLE_RULE_OPERATE
  needs_allocation = needs_allocation || interaction_rule_kind == INTERACTION_RULE_OPERATE;
#endif
#if ENABLE_RULE_SWITCH
  needs_allocation = needs_allocation || interaction_rule_kind == INTERACTION_RULE_SWITCH;
#endif
#if ENABLE_RULE_CALL
  needs_allocation = needs_allocation || interaction_rule_kind == INTERACTION_RULE_CALL;
#endif
  return needs_allocation;
}

static inline Port get_link_variable_port_for_conflict_check(Port left_port, Port right_port) {
  if (is_variable_port(left_port)) {
    return left_port;
  }
  if (is_variable_port(right_port)) {
    return right_port;
  }
  return make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
}

static inline ap_uint<32> get_substitution_memory_bank_index(Port variable_port) {
  return get_port_value(variable_port) % SUBSTITUTION_MEMORY_BANK_COUNT;
}

static inline ap_uint<32> get_node_memory_bank_index(Port binary_node_port) {
  return get_port_value(binary_node_port) % NODE_MEMORY_BANK_COUNT;
}

static inline ap_uint<1> link_rules_touch_same_substitution_bank(
    RedexPair first_active_pair,
    RedexPair second_active_pair) {
  Port first_variable_port = get_link_variable_port_for_conflict_check(
      first_active_pair.left_port,
      first_active_pair.right_port);
  Port second_variable_port = get_link_variable_port_for_conflict_check(
      second_active_pair.left_port,
      second_active_pair.right_port);

  if (!is_variable_port(first_variable_port) ||
      !is_variable_port(second_variable_port)) {
    return 0;
  }

  PortValue first_variable_index = get_port_value(first_variable_port);
  PortValue second_variable_index = get_port_value(second_variable_port);
  return first_variable_index != second_variable_index &&
         get_substitution_memory_bank_index(first_variable_port) ==
             get_substitution_memory_bank_index(second_variable_port);
}

static inline ap_uint<1> active_pairs_touch_same_binary_node(
    RedexPair first_active_pair,
    RedexPair second_active_pair) {
  Port first_ports[2] = {first_active_pair.left_port, first_active_pair.right_port};
  Port second_ports[2] = {second_active_pair.left_port, second_active_pair.right_port};

  for (int first_port_index = 0; first_port_index < 2; ++first_port_index) {
    for (int second_port_index = 0; second_port_index < 2; ++second_port_index) {
      if (is_binary_node_port(first_ports[first_port_index]) &&
          is_binary_node_port(second_ports[second_port_index]) &&
          get_port_value(first_ports[first_port_index]) == get_port_value(second_ports[second_port_index])) {
        return 1;
      }
    }
  }
  return 0;
}

static inline ap_uint<1> active_pairs_touch_same_node_bank(
    RedexPair first_active_pair,
    RedexPair second_active_pair) {
  Port first_ports[2] = {first_active_pair.left_port, first_active_pair.right_port};
  Port second_ports[2] = {second_active_pair.left_port, second_active_pair.right_port};

  for (int first_port_index = 0; first_port_index < 2; ++first_port_index) {
    for (int second_port_index = 0; second_port_index < 2; ++second_port_index) {
      if (is_binary_node_port(first_ports[first_port_index]) &&
          is_binary_node_port(second_ports[second_port_index])) {
        PortValue first_node_address = get_port_value(first_ports[first_port_index]);
        PortValue second_node_address = get_port_value(second_ports[second_port_index]);
        if (first_node_address != second_node_address &&
            get_node_memory_bank_index(first_ports[first_port_index]) ==
                get_node_memory_bank_index(second_ports[second_port_index])) {
          return 1;
        }
      }
    }
  }
  return 0;
}

static inline InteractionResult apply_lane_link_rule(
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

static inline InteractionResult apply_lane_erase_rule(
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
  if (!append_produced_active_pair(produced_active_pairs, erasure_port, get_first_auxiliary_port(node_data)) ||
      !append_produced_active_pair(produced_active_pairs, erasure_port, get_second_auxiliary_port(node_data))) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_annihilate_rule(
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
  if (!append_produced_active_pair(
          produced_active_pairs,
          get_first_auxiliary_port(left_node),
          get_first_auxiliary_port(right_node)) ||
      !append_produced_active_pair(
          produced_active_pairs,
          get_second_auxiliary_port(left_node),
          get_second_auxiliary_port(right_node))) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_operate_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
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
    if (!append_produced_active_pair(
            produced_active_pairs,
            produced_numeric_result_port,
            operator_second_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
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
  if (!append_produced_active_pair(
          produced_active_pairs,
          operator_first_auxiliary_port,
          delayed_operator_node_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_switch_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
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
    Port zero_branch_constructor_node_port =
        make_port(PORT_TAG_CONSTRUCTOR, zero_branch_constructor_node_address);
    if (!append_produced_active_pair(
            produced_active_pairs,
            zero_branch_constructor_node_port,
            switch_first_auxiliary_port)) {
      result.success = 0;
      result.queue_overflow = 1;
    }
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
  if (!append_produced_active_pair(
          produced_active_pairs,
          outer_constructor_node_port,
          switch_first_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_commute_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
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

  PortTag left_binary_node_tag = get_port_tag(left_port);
  PortTag right_binary_node_tag = get_port_tag(right_port);
  PackedNode left_node = node_memory[left_node_address];
  PackedNode right_node = node_memory[right_node_address];

  Port left_first_auxiliary_port = get_first_auxiliary_port(left_node);
  Port left_second_auxiliary_port = get_second_auxiliary_port(left_node);
  Port right_first_auxiliary_port = get_first_auxiliary_port(right_node);
  Port right_second_auxiliary_port = get_second_auxiliary_port(right_node);

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
      node_memory, allocated_node_count, fresh_variable_x_port, fresh_variable_y_port, &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_zw_address = allocate_node(
      node_memory, allocated_node_count, fresh_variable_z_port, fresh_variable_w_port, &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_xz_address = allocate_node(
      node_memory, allocated_node_count, fresh_variable_x_port, fresh_variable_z_port, &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }
  NodeAddress node_yw_address = allocate_node(
      node_memory, allocated_node_count, fresh_variable_y_port, fresh_variable_w_port, &allocation_successful);
  if (!allocation_successful) {
    result.success = 0;
    result.node_memory_exhausted = 1;
    return result;
  }

  if (!append_produced_active_pair(
          produced_active_pairs,
          make_port(right_binary_node_tag, node_ry_address),
          left_first_auxiliary_port) ||
      !append_produced_active_pair(
          produced_active_pairs,
          make_port(right_binary_node_tag, node_zw_address),
          left_second_auxiliary_port) ||
      !append_produced_active_pair(
          produced_active_pairs,
          make_port(left_binary_node_tag, node_xz_address),
          right_first_auxiliary_port) ||
      !append_produced_active_pair(
          produced_active_pairs,
          make_port(left_binary_node_tag, node_yw_address),
          right_second_auxiliary_port)) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_safe_reference_duplication_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    Port reference_port,
    Port duplicator_port) {
  InteractionResult result = make_interaction_result(1);
  NodeAddress duplicator_node_address = get_port_value(duplicator_port);
  if (duplicator_node_address >= MAXIMUM_NODE_COUNT) {
    result.success = 0;
    result.unsupported_interaction = 1;
    return result;
  }

  PackedNode duplicator_node = node_memory[duplicator_node_address];
  if (!append_produced_active_pair(
          produced_active_pairs,
          reference_port,
          get_first_auxiliary_port(duplicator_node)) ||
      !append_produced_active_pair(
          produced_active_pairs,
          reference_port,
          get_second_auxiliary_port(duplicator_node))) {
    result.success = 0;
    result.queue_overflow = 1;
  }
  return result;
}

static inline InteractionResult apply_lane_call_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
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
    return apply_lane_safe_reference_duplication_rule(
        node_memory,
        produced_active_pairs,
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
  if (definition_header.redex_count + 1 > MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION) {
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
          !append_produced_active_pair(
              produced_active_pairs,
              adjusted_definition_redex.left_port,
              adjusted_definition_redex.right_port)) {
        result.success = 0;
        result.queue_overflow = adjustment_successful;
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
  if (!adjustment_successful ||
      !append_produced_active_pair(
          produced_active_pairs,
          adjusted_root_port,
          call_argument_port)) {
    result.success = 0;
    result.queue_overflow = adjustment_successful;
    result.unsupported_interaction = !adjustment_successful;
  }
  return result;
}

static inline InteractionResult apply_lane_interaction_rule(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ProducedActivePairs* produced_active_pairs,
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    Port left_port,
    Port right_port,
    InteractionRuleKind interaction_rule_kind,
    ap_uint<32>* status_code,
    ReducerStatistics* reducer_statistics) {
  initialize_produced_active_pairs(produced_active_pairs);

  switch (interaction_rule_kind) {
    case INTERACTION_RULE_LINK:
      return apply_lane_link_rule(substitution_memory, produced_active_pairs, left_port, right_port);
    case INTERACTION_RULE_VOID:
      return apply_void_rule();
    case INTERACTION_RULE_ERASE:
      return apply_lane_erase_rule(node_memory, produced_active_pairs, left_port, right_port);
    case INTERACTION_RULE_ANNIHILATE:
      return apply_lane_annihilate_rule(node_memory, produced_active_pairs, left_port, right_port);
#if ENABLE_RULE_COMMUTE
    case INTERACTION_RULE_COMMUTE:
      return apply_lane_commute_rule(
          node_memory,
          substitution_memory,
          produced_active_pairs,
          allocated_node_count,
          allocated_variable_count,
          left_port,
          right_port);
#endif
#if ENABLE_RULE_OPERATE
    case INTERACTION_RULE_OPERATE:
      return apply_lane_operate_rule(
          node_memory,
          produced_active_pairs,
          allocated_node_count,
          left_port,
          right_port,
          status_code,
          reducer_statistics);
#endif
#if ENABLE_RULE_SWITCH
    case INTERACTION_RULE_SWITCH:
      return apply_lane_switch_rule(
          node_memory,
          produced_active_pairs,
          allocated_node_count,
          left_port,
          right_port,
          status_code);
#endif
#if ENABLE_RULE_CALL
    case INTERACTION_RULE_CALL:
      return apply_lane_call_rule(
          node_memory,
          substitution_memory,
          produced_active_pairs,
          book_definition_headers,
          book_redex_memory,
          book_node_memory,
          book_definition_count,
          allocated_node_count,
          allocated_variable_count,
          left_port,
          right_port);
#endif
    case INTERACTION_RULE_UNSUPPORTED:
    default: {
      InteractionResult result = make_interaction_result(0);
      result.unsupported_interaction = 1;
      return result;
    }
  }
}

#endif
