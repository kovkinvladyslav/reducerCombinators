#ifndef PROGRAM_BOOK_BUILDER_HPP
#define PROGRAM_BOOK_BUILDER_HPP

static inline Port book_var(ap_uint<32> local_variable_index) {
  return make_port(PORT_TAG_VARIABLE, local_variable_index);
}

static inline Port book_ref(ap_uint<32> definition_index) {
  return make_port(PORT_TAG_REFERENCE, definition_index);
}

static inline Port book_u24(ap_uint<24> value) {
  return make_u24_number_port(value);
}

static inline Port book_i24(ap_int<24> value) {
  return make_i24_number_port(value);
}

static inline Port book_con(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_CONSTRUCTOR, local_node_index);
}

static inline Port book_dup(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_DUPLICATOR, local_node_index);
}

static inline Port book_ope(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_OPERATOR, local_node_index);
}

static inline Port book_swi(ap_uint<32> local_node_index) {
  return make_port(PORT_TAG_SWITCH, local_node_index);
}

struct ProgramBookBuilder {
  BookDefinitionHeader* definition_headers;
  RedexPair* book_redex_memory;
  PackedNode* book_node_memory;
  ap_uint<32> definition_count;
  ap_uint<32> next_book_redex_index;
  ap_uint<32> next_book_node_index;
  ap_uint<32> active_definition_index;
  ap_uint<1> definition_is_open;
  ap_uint<1> builder_error;
};

static inline void initialize_program_book_builder(
    ProgramBookBuilder* builder,
    BookDefinitionHeader definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT]) {
  builder->definition_headers = definition_headers;
  builder->book_redex_memory = book_redex_memory;
  builder->book_node_memory = book_node_memory;
  builder->definition_count = 0;
  builder->next_book_redex_index = 0;
  builder->next_book_node_index = 0;
  builder->active_definition_index = 0;
  builder->definition_is_open = 0;
  builder->builder_error = 0;
}

static inline ap_uint<1> program_book_begin_definition(
    ProgramBookBuilder* builder,
    ap_uint<32> definition_index,
    ap_uint<1> definition_is_safe,
    Port root_port,
    ap_uint<32> variable_count) {
  if (definition_index >= MAXIMUM_BOOK_DEFINITION_COUNT ||
      variable_count > MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION) {
    builder->builder_error = 1;
    return 0;
  }

  BookDefinitionHeader header;
  header.definition_is_safe = definition_is_safe;
  header.root_port = root_port;
  header.redex_count = 0;
  header.node_count = 0;
  header.variable_count = variable_count;
  header.redex_base_index = builder->next_book_redex_index;
  header.node_base_index = builder->next_book_node_index;
  builder->definition_headers[definition_index] = header;
  builder->active_definition_index = definition_index;
  builder->definition_is_open = 1;
  if (definition_index + 1 > builder->definition_count) {
    builder->definition_count = definition_index + 1;
  }
  return 1;
}

static inline ap_uint<1> program_book_add_node(
    ProgramBookBuilder* builder,
    Port first_auxiliary_port,
    Port second_auxiliary_port,
    ap_uint<32>* local_node_index) {
  if (!builder->definition_is_open ||
      builder->next_book_node_index >= MAXIMUM_BOOK_NODE_COUNT) {
    builder->builder_error = 1;
    return 0;
  }
  BookDefinitionHeader* active_header = &builder->definition_headers[builder->active_definition_index];
  if (active_header->node_count >= MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION) {
    builder->builder_error = 1;
    return 0;
  }

  *local_node_index = active_header->node_count;
  builder->book_node_memory[builder->next_book_node_index] =
      make_packed_node(first_auxiliary_port, second_auxiliary_port);
  builder->next_book_node_index = builder->next_book_node_index + 1;
  active_header->node_count = active_header->node_count + 1;
  return 1;
}

static inline ap_uint<1> program_book_add_redex(
    ProgramBookBuilder* builder,
    Port left_port,
    Port right_port) {
  if (!builder->definition_is_open ||
      builder->next_book_redex_index >= MAXIMUM_BOOK_REDEX_COUNT) {
    builder->builder_error = 1;
    return 0;
  }
  BookDefinitionHeader* active_header = &builder->definition_headers[builder->active_definition_index];
  if (active_header->redex_count >= MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION) {
    builder->builder_error = 1;
    return 0;
  }

  builder->book_redex_memory[builder->next_book_redex_index].left_port = left_port;
  builder->book_redex_memory[builder->next_book_redex_index].right_port = right_port;
  builder->next_book_redex_index = builder->next_book_redex_index + 1;
  active_header->redex_count = active_header->redex_count + 1;
  return 1;
}

static inline void program_book_end_definition(ProgramBookBuilder* builder) {
  builder->definition_is_open = 0;
}

static inline ap_uint<1> program_book_succeeded(ProgramBookBuilder* builder) {
  return builder->builder_error == 0;
}

#endif
