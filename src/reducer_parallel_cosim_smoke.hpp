#ifndef REDUCER_PARALLEL_COSIM_SMOKE_HPP
#define REDUCER_PARALLEL_COSIM_SMOKE_HPP

#include "reducer_core_parallel.hpp"

extern "C" void reducer_parallel_cosim_smoke(
    ap_uint<32>* active_pair_count_out,
    ap_uint<32>* performed_steps_out,
    ap_uint<32>* status_code_out,
    ap_uint<32>* parallel_cycle_count_out,
    ap_uint<32>* multi_commit_cycle_count_out,
    ap_uint<32>* performed_void_count_out,
    ap_uint<32>* maximum_global_queue_count_out);

#endif
