#include "reducer_sum_tree_scaled_cosim.hpp"

#include "book_storage.hpp"
#include "numeric_encoding.hpp"

static void initialize_sum_tree_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    node_memory[index] = 0;
  }
}

static void initialize_sum_tree_substitution_memory(Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port empty_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int index = 0; index < MAXIMUM_VARIABLE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    substitution_memory[index] = empty_port;
  }
}

static void initialize_sum_tree_redex_memory(RedexPair redex_memory[MAXIMUM_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    redex_memory[index].left_port = empty_port;
    redex_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_tree_book_headers(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT]) {
  BookDefinitionHeader empty_header = make_empty_book_definition_header();
  for (int index = 0; index < MAXIMUM_BOOK_DEFINITION_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=16
    book_definition_headers[index] = empty_header;
  }
}

static void initialize_sum_tree_book_redex_memory(RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_BOOK_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_redex_memory[index].left_port = empty_port;
    book_redex_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_tree_book_node_memory(PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_BOOK_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_node_memory[index] = 0;
  }
}

static Port sum_tree_book_var(ap_uint<32> local_variable_index) {
  return make_port(PORT_TAG_VARIABLE, local_variable_index);
}

static Port sum_tree_book_ref(ap_uint<32> definition_index) {
  return make_port(PORT_TAG_REFERENCE, definition_index);
}

static Port sum_tree_book_con(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_CONSTRUCTOR, local_node_index);
}

static Port sum_tree_book_u24(ap_uint<24> value) {
  return make_u24_number_port(value);
}

static Port sum_tree_book_ope(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_OPERATOR, local_node_index);
}

static void build_sum_tree_scaled_book(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32>* book_definition_count) {
  BookDefinitionHeader header;
  header.definition_is_safe = 1;
  header.root_port = sum_tree_book_con(4);
  header.redex_count = 4;
  header.node_count = 7;
  header.variable_count = 4;
  header.redex_base_index = 0;
  header.node_base_index = 0;
  book_definition_headers[0] = header;
  *book_definition_count = 1;

  for (int pair_index = 0; pair_index < 4; ++pair_index) {
#pragma HLS LOOP_TRIPCOUNT min=4 max=4
    Port operation_port = make_numeric_operation_port(
        NUMERIC_TAG_OPERATION_ADD,
        NumericPayload(pair_index * 2));
    book_node_memory[pair_index] = make_packed_node(operation_port, sum_tree_book_var(pair_index));
    book_redex_memory[pair_index].left_port = sum_tree_book_u24((pair_index * 2) + 1);
    book_redex_memory[pair_index].right_port = sum_tree_book_ope(pair_index);
  }

  book_node_memory[4] = make_packed_node(sum_tree_book_var(0), sum_tree_book_con(5));
  book_node_memory[5] = make_packed_node(sum_tree_book_var(1), sum_tree_book_con(6));
  book_node_memory[6] = make_packed_node(sum_tree_book_var(2), sum_tree_book_var(3));
}

static ap_uint<1> read_sum_tree_frontier_sum(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ap_uint<32>* frontier_sum_out) {
  ap_uint<32> frontier_sum = 0;
  ap_uint<1> frontier_is_u24 = 1;
  for (int variable_index = 0; variable_index < 4; ++variable_index) {
#pragma HLS LOOP_TRIPCOUNT min=4 max=4
    Port frontier_port = substitution_memory[variable_index];
    ap_uint<1> is_u24 =
        get_port_tag(frontier_port) == PORT_TAG_NUMBER &&
        get_numeric_tag_from_port(frontier_port) == NUMERIC_TAG_U24;
    if (is_u24) {
      frontier_sum += get_u24_from_numeric_port(frontier_port);
    } else {
      frontier_is_u24 = 0;
    }
  }
  *frontier_sum_out = frontier_sum;
  return frontier_is_u24;
}

extern "C" void reducer_sum_tree_scaled_cosim(
    ap_uint<32>* frontier_sum_out,
    ap_uint<32>* root_constructor_address_out,
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* call_count_out,
    ap_uint<32>* operate_count_out,
    ap_uint<32>* max_queue_count_out,
    ap_uint<32>* max_node_count_out,
    ap_uint<32>* max_variable_count_out) {
  PackedNode node_memory[MAXIMUM_NODE_COUNT];
  Port substitution_memory[MAXIMUM_VARIABLE_COUNT];
  RedexPair active_pair_memory[MAXIMUM_REDEX_COUNT];
  BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT];
  RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT];
  PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT];
  ReducerStatistics reducer_statistics;

  initialize_sum_tree_node_memory(node_memory);
  initialize_sum_tree_substitution_memory(substitution_memory);
  initialize_sum_tree_redex_memory(active_pair_memory);
  initialize_sum_tree_book_headers(book_definition_headers);
  initialize_sum_tree_book_redex_memory(book_redex_memory);
  initialize_sum_tree_book_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  ap_uint<32> book_definition_count = 0;
  build_sum_tree_scaled_book(
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      &book_definition_count);

  active_pair_memory[0].left_port = sum_tree_book_ref(0);
  active_pair_memory[0].right_port = make_root_port();

  ap_uint<32> active_pair_count = 1;
  ap_uint<32> allocated_node_count = 0;
  ap_uint<32> allocated_variable_count = 0;
  ap_uint<32> performed_steps = 0;
  ap_uint<32> status_code = REDUCER_STATUS_SUCCESS;

  reducer_core(
      node_memory,
      substitution_memory,
      active_pair_memory,
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      book_definition_count,
      &reducer_statistics,
      &active_pair_count,
      &allocated_node_count,
      &allocated_variable_count,
      384,
      &performed_steps,
      &status_code);

  ap_uint<32> frontier_sum = 0;
  ap_uint<1> frontier_is_u24 = read_sum_tree_frontier_sum(substitution_memory, &frontier_sum);
  Port root_port = substitution_memory[ROOT_VARIABLE_INDEX];
  ap_uint<1> root_is_constructor =
      get_port_tag(root_port) == PORT_TAG_CONSTRUCTOR &&
      get_port_value(root_port) == 4;
  if ((!frontier_is_u24 || !root_is_constructor) && status_code == REDUCER_STATUS_SUCCESS) {
    status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  }

  *frontier_sum_out = frontier_sum;
  ap_uint<32> root_constructor_address = root_is_constructor
      ? ap_uint<32>(get_port_value(root_port))
      : ap_uint<32>(INVALID_PORT_VALUE);
  *root_constructor_address_out = root_constructor_address;
  *active_pair_count_out = active_pair_count;
  *performed_steps_out = performed_steps;
  *status_code_out = status_code;
  *call_count_out = reducer_statistics.performed_call_count;
  *operate_count_out = reducer_statistics.performed_operate_count;
  *max_queue_count_out = reducer_statistics.maximum_observed_redex_queue_count;
  *max_node_count_out = reducer_statistics.maximum_observed_allocated_node_count;
  *max_variable_count_out = reducer_statistics.maximum_observed_allocated_variable_count;
}
