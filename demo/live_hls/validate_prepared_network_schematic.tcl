set script_dir [file dirname [file normalize [info script]]]
set rtl_dir [file join $script_dir {interactReduce_live_demo/hls/syn/verilog}]
set project_dir [file join $script_dir {vivado_prepared_network_schematic_check}]
set top_name {parallel_reducer_live_demo_top}
create_project -force prepared_network_reducer_schematic_check $project_dir -part xc7a35tcpg236-1
foreach ip_script [glob -nocomplain [file join $rtl_dir *_ip.tcl]] { source $ip_script }
set rtl_files [glob -nocomplain [file join $rtl_dir *.v]]
if {[llength $rtl_files] == 0} { error "No Verilog files found in $rtl_dir. Run run_live_demo_csynth.bat first." }
add_files $rtl_files
set_property top $top_name [current_fileset]
update_compile_order -fileset sources_1
synth_design -rtl -top $top_name -part xc7a35tcpg236-1
set reports_dir [file join $script_dir reports]
file mkdir $reports_dir
set marker_file [file join $reports_dir {prepared_network_elaborated_ok.txt}]
set marker [open $marker_file w]
puts $marker "RTL elaboration completed for $top_name"
close $marker
exit
