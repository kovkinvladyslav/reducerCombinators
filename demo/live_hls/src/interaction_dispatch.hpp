#ifndef INTERACTION_DISPATCH_HPP
#define INTERACTION_DISPATCH_HPP

#include "port_encoding.hpp"
#include "numeric_encoding.hpp"
#include "interaction_rule_kind.hpp"

static inline ap_uint<1> is_supported_port_tag(PortTag port_tag) {
  return port_tag <= PORT_TAG_SWITCH;
}

static inline InteractionRuleKind get_interaction_rule_kind(Port left_port, Port right_port) {
  PortTag left_port_tag = get_port_tag(left_port);
  PortTag right_port_tag = get_port_tag(right_port);

  if (!is_supported_port_tag(left_port_tag) || !is_supported_port_tag(right_port_tag)) {
    return INTERACTION_RULE_UNSUPPORTED;
  }

#if ENABLE_RULE_CALL
  if (left_port_tag == PORT_TAG_REFERENCE || right_port_tag == PORT_TAG_REFERENCE) {
    if (is_root_port(left_port) || is_root_port(right_port)) {
      return INTERACTION_RULE_CALL;
    }
  }
#endif

#if ENABLE_RULE_LINK
  if (is_variable_port(left_port) || is_variable_port(right_port)) {
    return INTERACTION_RULE_LINK;
  }
#endif

#if ENABLE_RULE_CALL
  if (left_port_tag == PORT_TAG_REFERENCE || right_port_tag == PORT_TAG_REFERENCE) {
    return INTERACTION_RULE_CALL;
  }
#endif

  ap_uint<1> left_is_numeric_port = (get_port_tag(left_port) == PORT_TAG_NUMBER);
  ap_uint<1> right_is_numeric_port = (get_port_tag(right_port) == PORT_TAG_NUMBER);
  ap_uint<1> left_is_operator_port = (get_port_tag(left_port) == PORT_TAG_OPERATOR);
  ap_uint<1> right_is_operator_port = (get_port_tag(right_port) == PORT_TAG_OPERATOR);
  ap_uint<1> left_is_switch_port = (get_port_tag(left_port) == PORT_TAG_SWITCH);
  ap_uint<1> right_is_switch_port = (get_port_tag(right_port) == PORT_TAG_SWITCH);

#if ENABLE_RULE_OPERATE
  if ((left_is_numeric_port && right_is_operator_port) ||
      (right_is_numeric_port && left_is_operator_port)) {
    return INTERACTION_RULE_OPERATE;
  }
#endif

#if ENABLE_RULE_SWITCH
  if ((left_is_numeric_port && right_is_switch_port) ||
      (right_is_numeric_port && left_is_switch_port)) {
    return INTERACTION_RULE_SWITCH;
  }
#endif

  if (is_nullary_node_port(left_port) && is_nullary_node_port(right_port)) {
    return INTERACTION_RULE_VOID;
  }

#if ENABLE_RULE_ERASE
  if ((is_erasure_port(left_port) && is_binary_node_port(right_port)) ||
      (is_erasure_port(right_port) && is_binary_node_port(left_port))) {
    return INTERACTION_RULE_ERASE;
  }
#endif

  if (is_binary_node_port(left_port) && is_binary_node_port(right_port)) {
#if ENABLE_RULE_ANNIHILATE
    if (ports_have_same_tag(left_port, right_port)) {
      return INTERACTION_RULE_ANNIHILATE;
    }
#endif
#if ENABLE_RULE_COMMUTE
    return INTERACTION_RULE_COMMUTE;
#endif
  }

  return INTERACTION_RULE_UNSUPPORTED;
}

#endif
