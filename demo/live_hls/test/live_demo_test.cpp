#include "../src/parallel_reducer_live_demo_top.hpp"
#include "../src/prepared_network_format.hpp"

#include <iostream>

static void print_prepared_pair(unsigned int index, const PreparedActivePairInput& pair) {
  std::cout << index << ",U24(" << pair.active_pair_number << "),OPE("
            << pair.operator_id << ")," << pair.left_value << "," << pair.right_value
            << ",VAR(" << pair.result_variable << ")\n";
}

static void print_journal_row(
    unsigned int cycle,
    unsigned int channel,
    const PreparedActivePairInput& pair) {
  std::cout << cycle << "," << channel << ",U24(" << pair.active_pair_number
            << ") <-> OPE(" << pair.operator_id << ")," << pair.left_value << " + "
            << pair.right_value << ",VAR(" << pair.result_variable << "),"
            << pair.left_value + pair.right_value << "\n";
}

int main() {
  PreparedActivePairInput network[PREPARED_PAIR_COUNT] = {
      {1, 0, 0, 1, 0},
      {3, 1, 2, 3, 1},
      {5, 2, 4, 5, 2},
      {7, 3, 6, 7, 3},
      {9, 4, 8, 9, 4},
      {11, 5, 10, 11, 5},
      {13, 6, 12, 13, 6},
      {15, 7, 14, 15, 7},
  };

  ap_uint<32> live_valid = 0;
  ap_uint<32> live_cycle = 0;
  ap_uint<32> channel0_pair_number = 0;
  ap_uint<32> channel0_operator = 0;
  ap_uint<32> channel0_left = 0;
  ap_uint<32> channel0_right = 0;
  ap_uint<32> channel0_variable = 0;
  ap_uint<32> channel0_result = 0;
  ap_uint<32> channel1_pair_number = 0;
  ap_uint<32> channel1_operator = 0;
  ap_uint<32> channel1_left = 0;
  ap_uint<32> channel1_right = 0;
  ap_uint<32> channel1_variable = 0;
  ap_uint<32> channel1_result = 0;
  ap_uint<32> channel2_pair_number = 0;
  ap_uint<32> channel2_operator = 0;
  ap_uint<32> channel2_left = 0;
  ap_uint<32> channel2_right = 0;
  ap_uint<32> channel2_variable = 0;
  ap_uint<32> channel2_result = 0;
  ap_uint<32> channel3_pair_number = 0;
  ap_uint<32> channel3_operator = 0;
  ap_uint<32> channel3_left = 0;
  ap_uint<32> channel3_right = 0;
  ap_uint<32> channel3_variable = 0;
  ap_uint<32> channel3_result = 0;
  ap_uint<32> final_result = 0;

  parallel_reducer_live_demo_top(
      network,
      &live_valid,
      &live_cycle,
      &channel0_pair_number,
      &channel0_operator,
      &channel0_left,
      &channel0_right,
      &channel0_variable,
      &channel0_result,
      &channel1_pair_number,
      &channel1_operator,
      &channel1_left,
      &channel1_right,
      &channel1_variable,
      &channel1_result,
      &channel2_pair_number,
      &channel2_operator,
      &channel2_left,
      &channel2_right,
      &channel2_variable,
      &channel2_result,
      &channel3_pair_number,
      &channel3_operator,
      &channel3_left,
      &channel3_right,
      &channel3_variable,
      &channel3_result,
      &final_result);

  std::cout << "Prepared active-pair table\n";
  std::cout << "row,active_pair_number,operator,left,right,result_variable\n";
  for (unsigned int index = 0; index < PREPARED_PAIR_COUNT; ++index) {
    print_prepared_pair(index, network[index]);
  }

  std::cout << "\nParallel reduction journal\n";
  std::cout << "cycle,channel,active_pair,operation,result_variable,result_value\n";
  for (unsigned int index = 0; index < PREPARED_PAIR_COUNT; ++index) {
    print_journal_row(index / PREPARED_CHANNEL_COUNT + 1, index % PREPARED_CHANNEL_COUNT,
                      network[index]);
  }

  std::cout << "\nFinal pin sample\n";
  std::cout << "live_valid=" << live_valid << "\n";
  std::cout << "live_cycle=" << live_cycle << "\n";
  std::cout << "final_result=" << final_result << "\n";

  if (live_valid != 1) return 1;
  if (live_cycle != 2) return 1;
  if (channel0_pair_number != 9 || channel0_operator != 4 || channel0_variable != 4) return 1;
  if (channel0_result != 17) return 1;
  if (channel1_pair_number != 11 || channel1_operator != 5 || channel1_variable != 5) return 1;
  if (channel1_result != 21) return 1;
  if (channel2_pair_number != 13 || channel2_operator != 6 || channel2_variable != 6) return 1;
  if (channel2_result != 25) return 1;
  if (channel3_pair_number != 15 || channel3_operator != 7 || channel3_variable != 7) return 1;
  if (channel3_result != 29) return 1;
  if (final_result != 120) return 1;
  return 0;
}


