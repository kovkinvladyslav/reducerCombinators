#ifndef PREPARED_NETWORK_FORMAT_HPP
#define PREPARED_NETWORK_FORMAT_HPP

#include <ap_int.h>

struct PreparedActivePairInput {
  ap_uint<32> active_pair_number;
  ap_uint<32> operator_id;
  ap_uint<32> left_value;
  ap_uint<32> right_value;
  ap_uint<32> result_variable;
};

static const int PREPARED_PAIR_COUNT = 8;
static const int PREPARED_CHANNEL_COUNT = 4;

#endif
