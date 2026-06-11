#ifndef REDUCER_SUM_REC_SCALED_COSIM_HPP
#define REDUCER_SUM_REC_SCALED_COSIM_HPP

#include "reducer_core.hpp"

extern "C" void reducer_sum_rec_scaled_cosim(
    ap_uint<32>* root_value_out,
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* call_count_out,
    ap_uint<32>* operate_count_out,
    ap_uint<32>* max_queue_count_out,
    ap_uint<32>* max_node_count_out,
    ap_uint<32>* max_variable_count_out);

#endif
