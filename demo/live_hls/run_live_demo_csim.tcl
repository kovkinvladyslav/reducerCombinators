set script_dir [file dirname [file normalize [info script]]]
open_component interactReduce_live_demo -config=[file join $script_dir hls_live_demo_config.cfg] -apply_ini=true
csim_design
close_component
exit
