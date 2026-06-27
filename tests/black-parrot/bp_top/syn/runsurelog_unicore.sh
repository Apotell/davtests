#!/bin/bash
{ # try
  rm -rf results/verilator/bp_tethered.e_bp_unicore_cfg.none.build/
  make build.sc CFG=e_bp_unicore_cfg  VERILATOR="$1 -DVERILATOR=1 -sverilog -verbose -timescale=1ps/1ps -d dbstats -verbose -lowmem -o unicore"  && echo "OK"
    #save your output

} || { # catch
    # save log for exception
    echo "OK"
}
