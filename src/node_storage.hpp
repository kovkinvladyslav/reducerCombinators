#ifndef NODE_STORAGE_HPP
#define NODE_STORAGE_HPP

#include "port_encoding.hpp"

static inline PackedNode make_packed_node(Port first_auxiliary_port, Port second_auxiliary_port) {
  return (PackedNode(second_auxiliary_port) << PORT_WIDTH) | PackedNode(first_auxiliary_port);
}

static inline Port get_first_auxiliary_port(PackedNode node) {
  return Port(node.range(PORT_WIDTH - 1, 0));
}

static inline Port get_second_auxiliary_port(PackedNode node) {
  return Port(node.range((2 * PORT_WIDTH) - 1, PORT_WIDTH));
}

static inline PackedNode set_first_auxiliary_port(PackedNode node, Port first_auxiliary_port) {
  PackedNode updated_node = node;
  updated_node.range(PORT_WIDTH - 1, 0) = first_auxiliary_port;
  return updated_node;
}

static inline PackedNode set_second_auxiliary_port(PackedNode node, Port second_auxiliary_port) {
  PackedNode updated_node = node;
  updated_node.range((2 * PORT_WIDTH) - 1, PORT_WIDTH) = second_auxiliary_port;
  return updated_node;
}

static inline NodeAddress allocate_node(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    ap_uint<32>* allocated_node_count,
    Port first_auxiliary_port,
    Port second_auxiliary_port,
    ap_uint<1>* allocation_successful) {
  (void)node_memory;
  if (*allocated_node_count >= MAXIMUM_NODE_COUNT) {
    *allocation_successful = 0;
    return INVALID_NODE_ADDRESS;
  }

  NodeAddress allocated_node_address = NodeAddress(*allocated_node_count);
  node_memory[allocated_node_address] = make_packed_node(first_auxiliary_port, second_auxiliary_port);
  *allocated_node_count = *allocated_node_count + 1;
  *allocation_successful = 1;
  return allocated_node_address;
}

#endif
