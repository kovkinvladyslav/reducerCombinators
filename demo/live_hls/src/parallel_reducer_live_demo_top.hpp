#ifndef PARALLEL_REDUCER_LIVE_DEMO_TOP_HPP
#define PARALLEL_REDUCER_LIVE_DEMO_TOP_HPP

#include <ap_int.h>

#include "prepared_network_format.hpp"

void parallel_reducer_live_demo_top(
    const PreparedActivePairInput prepared_pairs[PREPARED_PAIR_COUNT],
    volatile ap_uint<32>* live_valid_out,
    volatile ap_uint<32>* live_cycle_out,
    volatile ap_uint<32>* channel0_active_pair_number_out,
    volatile ap_uint<32>* channel0_active_pair_operator_out,
    volatile ap_uint<32>* channel0_left_out,
    volatile ap_uint<32>* channel0_right_out,
    volatile ap_uint<32>* channel0_result_variable_out,
    volatile ap_uint<32>* channel0_result_out,
    volatile ap_uint<32>* channel1_active_pair_number_out,
    volatile ap_uint<32>* channel1_active_pair_operator_out,
    volatile ap_uint<32>* channel1_left_out,
    volatile ap_uint<32>* channel1_right_out,
    volatile ap_uint<32>* channel1_result_variable_out,
    volatile ap_uint<32>* channel1_result_out,
    volatile ap_uint<32>* channel2_active_pair_number_out,
    volatile ap_uint<32>* channel2_active_pair_operator_out,
    volatile ap_uint<32>* channel2_left_out,
    volatile ap_uint<32>* channel2_right_out,
    volatile ap_uint<32>* channel2_result_variable_out,
    volatile ap_uint<32>* channel2_result_out,
    volatile ap_uint<32>* channel3_active_pair_number_out,
    volatile ap_uint<32>* channel3_active_pair_operator_out,
    volatile ap_uint<32>* channel3_left_out,
    volatile ap_uint<32>* channel3_right_out,
    volatile ap_uint<32>* channel3_result_variable_out,
    volatile ap_uint<32>* channel3_result_out,
    volatile ap_uint<32>* final_result_out);

#endif
