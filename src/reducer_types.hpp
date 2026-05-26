#ifndef REDUCER_TYPES_HPP
#define REDUCER_TYPES_HPP

#include "reducer_config.hpp"

using Port = ap_uint<PORT_WIDTH>;
using PortTag = ap_uint<PORT_TAG_BIT_COUNT>;
using PortValue = ap_uint<PORT_VALUE_BIT_COUNT>;
using PackedNode = ap_uint<NODE_WIDTH>;
using NodeAddress = ap_uint<PORT_VALUE_BIT_COUNT>;
using ProducedPairCount = ap_uint<PRODUCED_PAIR_COUNTER_BIT_COUNT>;
using ReducerServicePhase = ap_uint<2>;

struct RedexPair {
  Port left_port;
  Port right_port;
};

struct ProducedActivePairs {
  RedexPair produced_pairs[MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION];
  ProducedPairCount produced_pair_count;
};

struct BookDefinitionHeader {
  ap_uint<1> definition_is_safe;
  Port root_port;
  ap_uint<32> redex_count;
  ap_uint<32> node_count;
  ap_uint<32> variable_count;
  ap_uint<32> redex_base_index;
  ap_uint<32> node_base_index;
};

struct CallInstantiationState {
  ap_uint<1> active;
  ap_uint<3> phase;
  Port reference_port;
  Port call_argument_port;
  BookDefinitionHeader definition_header;
  ap_uint<32> fresh_node_base_index;
  ap_uint<32> fresh_variable_base_index;
  ap_uint<32> variable_index;
  ap_uint<32> node_index;
  ap_uint<32> redex_index;
};

struct ReducerLaneState {
  RedexPair local_active_pair_queue[LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH];
  ap_uint<32> local_queue_head_index;
  ap_uint<32> local_queue_tail_index;
  ap_uint<32> local_queue_count;
  ap_uint<1> lane_is_idle;
};

struct InteractionResult {
  ap_uint<1> success;
  ap_uint<1> unsupported_interaction;
  ap_uint<1> queue_overflow;
  ap_uint<1> node_memory_exhausted;
  ap_uint<1> variable_memory_exhausted;
  ap_uint<1> invalid_port_tag;
  ap_uint<1> invalid_numeric_operation;
  ap_uint<1> numeric_division_by_zero;
  ap_uint<1> unsupported_numeric_type;
  ap_uint<1> unknown_reference;
  ap_uint<1> book_node_overflow;
  ap_uint<1> book_redex_overflow;
};

#endif
