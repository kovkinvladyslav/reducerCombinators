#ifndef BOOK_STORAGE_HPP
#define BOOK_STORAGE_HPP

#include "node_storage.hpp"

static inline Port make_root_port() {
  return make_port(PORT_TAG_VARIABLE, ROOT_VARIABLE_INDEX);
}

static inline BookDefinitionHeader make_empty_book_definition_header() {
  BookDefinitionHeader header;
  header.definition_is_safe = 0;
  header.root_port = make_port(PORT_TAG_ERASURE, 0);
  header.redex_count = 0;
  header.node_count = 0;
  header.variable_count = 0;
  header.redex_base_index = 0;
  header.node_base_index = 0;
  return header;
}

static inline Port adjust_book_port(
    Port definition_local_port,
    ap_uint<32> fresh_node_base_index,
    ap_uint<32> fresh_variable_base_index,
    ap_uint<32> definition_node_count,
    ap_uint<32> definition_variable_count,
    ap_uint<1>* adjustment_successful) {
  PortTag definition_local_port_tag = get_port_tag(definition_local_port);
  PortValue definition_local_port_value = get_port_value(definition_local_port);

  if (definition_local_port_tag == PORT_TAG_VARIABLE) {
    if (definition_local_port_value >= definition_variable_count) {
      *adjustment_successful = 0;
      return definition_local_port;
    }
    return make_port(PORT_TAG_VARIABLE, fresh_variable_base_index + definition_local_port_value);
  }

  if (is_binary_node_port(definition_local_port)) {
    if (definition_local_port_value >= definition_node_count) {
      *adjustment_successful = 0;
      return definition_local_port;
    }
    return make_port(definition_local_port_tag, fresh_node_base_index + definition_local_port_value);
  }

  return definition_local_port;
}

static inline RedexPair adjust_book_redex(
    RedexPair definition_local_redex,
    ap_uint<32> fresh_node_base_index,
    ap_uint<32> fresh_variable_base_index,
    ap_uint<32> definition_node_count,
    ap_uint<32> definition_variable_count,
    ap_uint<1>* adjustment_successful) {
  RedexPair adjusted_redex;
  adjusted_redex.left_port = adjust_book_port(
      definition_local_redex.left_port,
      fresh_node_base_index,
      fresh_variable_base_index,
      definition_node_count,
      definition_variable_count,
      adjustment_successful);
  adjusted_redex.right_port = adjust_book_port(
      definition_local_redex.right_port,
      fresh_node_base_index,
      fresh_variable_base_index,
      definition_node_count,
      definition_variable_count,
      adjustment_successful);
  return adjusted_redex;
}

#endif
