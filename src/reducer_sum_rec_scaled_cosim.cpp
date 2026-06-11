#include "reducer_sum_rec_scaled_cosim.hpp"

#include "book_storage.hpp"
#include "numeric_encoding.hpp"

#ifndef SUM_REC_OPERATION_COUNT
#define SUM_REC_OPERATION_COUNT 100
#endif

#ifndef SUM_REC_MAXIMUM_STEPS
#define SUM_REC_MAXIMUM_STEPS (SUM_REC_OPERATION_COUNT * 4 + 64)
#endif

static void initialize_sum_rec_node_memory(PackedNode node_memory[MAXIMUM_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    node_memory[index] = 0;
  }
}

static void initialize_sum_rec_substitution_memory(Port substitution_memory[MAXIMUM_VARIABLE_COUNT]) {
  Port empty_port = make_port(PORT_TAG_REFERENCE, INVALID_PORT_VALUE);
  for (int index = 0; index < MAXIMUM_VARIABLE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    substitution_memory[index] = empty_port;
  }
}

static void initialize_sum_rec_redex_memory(RedexPair redex_memory[MAXIMUM_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024
    redex_memory[index].left_port = empty_port;
    redex_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_rec_book_headers(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT]) {
  BookDefinitionHeader empty_header = make_empty_book_definition_header();
  for (int index = 0; index < MAXIMUM_BOOK_DEFINITION_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=16
    book_definition_headers[index] = empty_header;
  }
}

static void initialize_sum_rec_book_redex_memory(RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT]) {
  Port empty_port = make_port(PORT_TAG_ERASURE, 0);
  for (int index = 0; index < MAXIMUM_BOOK_REDEX_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_redex_memory[index].left_port = empty_port;
    book_redex_memory[index].right_port = empty_port;
  }
}

static void initialize_sum_rec_book_node_memory(PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  for (int index = 0; index < MAXIMUM_BOOK_NODE_COUNT; ++index) {
#pragma HLS LOOP_TRIPCOUNT min=128 max=128
    book_node_memory[index] = 0;
  }
}

static Port sum_rec_book_var(ap_uint<32> local_variable_index) {
  return make_port(PORT_TAG_VARIABLE, local_variable_index);
}

static Port sum_rec_book_ref(ap_uint<32> definition_index) {
  return make_port(PORT_TAG_REFERENCE, definition_index);
}

static Port sum_rec_book_u24(ap_uint<24> value) {
  return make_u24_number_port(value);
}

static Port sum_rec_book_ope(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_OPERATOR, local_node_index);
}

static void build_sum_rec_scaled_book(
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32>* book_definition_count) {
  const int operation_count = SUM_REC_OPERATION_COUNT;

  BookDefinitionHeader header;
  header.definition_is_safe = 1;
  header.root_port = sum_rec_book_var(operation_count - 1);
  header.redex_count = operation_count;
  header.node_count = operation_count;
  header.variable_count = operation_count;
  header.redex_base_index = 0;
  header.node_base_index = 0;
  book_definition_headers[0] = header;
  *book_definition_count = 1;

  for (int operation_index = 0; operation_index < operation_count; ++operation_index) {
#pragma HLS LOOP_TRIPCOUNT min=SUM_REC_OPERATION_COUNT max=SUM_REC_OPERATION_COUNT
    Port operation_port = make_numeric_operation_port(
        NUMERIC_TAG_OPERATION_ADD,
        NumericPayload(operation_index + 1));
    Port target_variable_port = sum_rec_book_var(operation_index);
    book_node_memory[operation_index] = make_packed_node(operation_port, target_variable_port);

    Port active_input_port = operation_index == 0
        ? sum_rec_book_u24(0)
        : sum_rec_book_var(operation_index - 1);
    book_redex_memory[operation_index].left_port = active_input_port;
    book_redex_memory[operation_index].right_port = sum_rec_book_ope(operation_index);
  }
}

static ap_uint<1> read_sum_rec_root_u24(
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    ap_uint<32>* root_value_out) {
  Port root_port = substitution_memory[ROOT_VARIABLE_INDEX];
  if (get_port_tag(root_port) != PORT_TAG_NUMBER ||
      get_numeric_tag_from_port(root_port) != NUMERIC_TAG_U24) {
    *root_value_out = 0;
    return 0;
  }
  *root_value_out = get_u24_from_numeric_port(root_port);
  return 1;
}

extern "C" void reducer_sum_rec_scaled_cosim(
    ap_uint<32>* root_value_out,
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

  initialize_sum_rec_node_memory(node_memory);
  initialize_sum_rec_substitution_memory(substitution_memory);
  initialize_sum_rec_redex_memory(active_pair_memory);
  initialize_sum_rec_book_headers(book_definition_headers);
  initialize_sum_rec_book_redex_memory(book_redex_memory);
  initialize_sum_rec_book_node_memory(book_node_memory);
  clear_reducer_statistics(&reducer_statistics);

  ap_uint<32> book_definition_count = 0;
  build_sum_rec_scaled_book(
      book_definition_headers,
      book_redex_memory,
      book_node_memory,
      &book_definition_count);

  active_pair_memory[0].left_port = sum_rec_book_ref(0);
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
      SUM_REC_MAXIMUM_STEPS,
      &performed_steps,
      &status_code);

  ap_uint<32> root_value = 0;
  ap_uint<1> root_is_u24 = read_sum_rec_root_u24(substitution_memory, &root_value);
  if (!root_is_u24 && status_code == REDUCER_STATUS_SUCCESS) {
    status_code = REDUCER_STATUS_UNSUPPORTED_INTERACTION;
  }

  *root_value_out = root_value;
  *active_pair_count_out = active_pair_count;
  *performed_steps_out = performed_steps;
  *status_code_out = status_code;
  *call_count_out = reducer_statistics.performed_call_count;
  *operate_count_out = reducer_statistics.performed_operate_count;
  *max_queue_count_out = reducer_statistics.maximum_observed_redex_queue_count;
  *max_node_count_out = reducer_statistics.maximum_observed_allocated_node_count;
  *max_variable_count_out = reducer_statistics.maximum_observed_allocated_variable_count;
}
