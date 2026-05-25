#ifndef REDUCER_CORE_PARALLEL_HPP
#define REDUCER_CORE_PARALLEL_HPP

#include "lane_interaction.hpp"

extern "C" void reducer_core_parallel(
    PackedNode node_memory[MAXIMUM_NODE_COUNT],
    Port substitution_memory[MAXIMUM_VARIABLE_COUNT],
    RedexPair global_active_pair_memory[MAXIMUM_REDEX_COUNT],
    BookDefinitionHeader book_definition_headers[MAXIMUM_BOOK_DEFINITION_COUNT],
    RedexPair book_redex_memory[MAXIMUM_BOOK_REDEX_COUNT],
    PackedNode book_node_memory[MAXIMUM_BOOK_NODE_COUNT],
    ap_uint<32> book_definition_count,
    ReducerStatistics* reducer_statistics,
    ap_uint<32>* active_pair_count,
    ap_uint<32>* allocated_node_count,
    ap_uint<32>* allocated_variable_count,
    ap_uint<32> maximum_steps,
    ap_uint<32>* performed_steps,
    ap_uint<32>* status_code);

#endif
