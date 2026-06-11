#ifndef REDUCER_SUM_TREE_PARALLEL_COSIM_HPP
#define REDUCER_SUM_TREE_PARALLEL_COSIM_HPP

#include "reducer_core_parallel.hpp"

extern "C" void reducer_sum_tree_parallel_cosim(
    ap_uint<32>* result_value_out,
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* operate_count_out,
    ap_uint<32>* parallel_cycle_count_out,
    ap_uint<32>* multi_commit_cycle_count_out,
    ap_uint<32>* maximum_global_queue_count_out,
    ap_uint<32>* max_node_count_out,
    ap_uint<32>* max_variable_count_out);

#endif
