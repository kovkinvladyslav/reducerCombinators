#ifndef REDUCER_CORE_HPP
#define REDUCER_CORE_HPP

#include "interaction_dispatch.hpp"
#include "interaction_rules.hpp"
#include "reducer_statistics.hpp"

extern "C" void reducer_core(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair redex_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* redex_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    ap_uint<32>* performed_steps,
    ap_uint<32>* status_code);

#endif
