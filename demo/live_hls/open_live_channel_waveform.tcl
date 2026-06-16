set script_dir [file dirname [file normalize [info script]]]
set wdb_file [file join $script_dir {interactReduce_live_demo/hls/sim/verilog/parallel_reducer_live_demo_top.wdb}]
if {![file exists $wdb_file]} {
  error "Waveform database not found: $wdb_file. Run run_live_demo_cosim.bat first."
}
open_wave_database $wdb_file

if {[llength [get_wave_configs live_channel_journal_v1 -quiet]] == 0} {
  create_wave_config live_channel_journal_v1
}
current_wave_config live_channel_journal_v1

set top {/apatb_parallel_reducer_live_demo_top_top/AESL_inst_parallel_reducer_live_demo_top}

proc add_unsigned_signal_if_exists {signal_path} {
  set found_signal [get_objects $signal_path -quiet]
  if {[llength $found_signal] > 0} {
    add_wave $found_signal -radix unsigned
  } else {
    puts "Signal not found: $signal_path"
  }
}

proc add_signal_if_exists {signal_path} {
  set found_signal [get_objects $signal_path -quiet]
  if {[llength $found_signal] > 0} {
    add_wave $found_signal
  } else {
    puts "Signal not found: $signal_path"
  }
}

add_signal_if_exists ${top}/prepared_pairs_ce0
add_unsigned_signal_if_exists ${top}/prepared_pairs_address0
add_unsigned_signal_if_exists ${top}/prepared_pairs_q0
add_signal_if_exists ${top}/prepared_pairs_ce1
add_unsigned_signal_if_exists ${top}/prepared_pairs_address1
add_unsigned_signal_if_exists ${top}/prepared_pairs_q1

for {set channel 0} {$channel < 4} {incr channel} {
  add_unsigned_signal_if_exists ${top}/channel${channel}_active_pair_number_out
  add_unsigned_signal_if_exists ${top}/channel${channel}_active_pair_operator_out
  add_unsigned_signal_if_exists ${top}/channel${channel}_left_out
  add_unsigned_signal_if_exists ${top}/channel${channel}_right_out
  add_unsigned_signal_if_exists ${top}/channel${channel}_result_variable_out
  add_unsigned_signal_if_exists ${top}/channel${channel}_result_out
}

add_unsigned_signal_if_exists ${top}/live_valid_out
add_unsigned_signal_if_exists ${top}/live_cycle_out
add_signal_if_exists ${top}/final_result_out_ap_vld
add_unsigned_signal_if_exists ${top}/final_result_out
add_unsigned_signal_if_exists ${top}/status_code_out
add_unsigned_signal_if_exists ${top}/operate_count_out
add_unsigned_signal_if_exists ${top}/parallel_cycle_count_out
add_unsigned_signal_if_exists ${top}/multi_commit_cycle_count_out

add_signal_if_exists ${top}/ap_clk
add_signal_if_exists ${top}/ap_rst
add_signal_if_exists ${top}/ap_start
add_signal_if_exists ${top}/ap_done
add_signal_if_exists ${top}/ap_idle
add_signal_if_exists ${top}/ap_ready
