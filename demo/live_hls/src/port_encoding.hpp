#ifndef PORT_ENCODING_HPP
#define PORT_ENCODING_HPP

#include "reducer_types.hpp"

static const ap_uint<PORT_WIDTH> PORT_TAG_MASK = (1u << PORT_TAG_BIT_COUNT) - 1u;
static const ap_uint<PORT_WIDTH> PORT_VALUE_MASK = (1u << PORT_VALUE_BIT_COUNT) - 1u;

static inline Port make_port(PortTag port_tag, PortValue port_value) {
  return (Port(port_value) << PORT_TAG_BIT_COUNT) | Port(port_tag);
}

static inline PortTag get_port_tag(Port port) {
  return PortTag(port & PORT_TAG_MASK);
}

static inline PortValue get_port_value(Port port) {
  return PortValue((port >> PORT_TAG_BIT_COUNT) & PORT_VALUE_MASK);
}

static inline ap_uint<1> is_variable_port(Port port) {
  return get_port_tag(port) == PORT_TAG_VARIABLE;
}

static inline ap_uint<1> is_root_port(Port port) {
  return get_port_tag(port) == PORT_TAG_VARIABLE &&
         get_port_value(port) == ROOT_VARIABLE_INDEX;
}

static inline ap_uint<1> is_erasure_port(Port port) {
  return get_port_tag(port) == PORT_TAG_ERASURE;
}

static inline ap_uint<1> is_binary_node_port(Port port) {
  PortTag port_tag = get_port_tag(port);
  return (port_tag == PORT_TAG_CONSTRUCTOR) || (port_tag == PORT_TAG_DUPLICATOR) ||
         (port_tag == PORT_TAG_OPERATOR) || (port_tag == PORT_TAG_SWITCH);
}

static inline ap_uint<1> is_nullary_node_port(Port port) {
  PortTag port_tag = get_port_tag(port);
  return (port_tag == PORT_TAG_ERASURE) || (port_tag == PORT_TAG_NUMBER) ||
         (port_tag == PORT_TAG_REFERENCE);
}

static inline ap_uint<1> ports_have_same_tag(Port first_port, Port second_port) {
  return get_port_tag(first_port) == get_port_tag(second_port);
}

#endif
