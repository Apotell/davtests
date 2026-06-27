#!/usr/bin/env bash

make RISCV=blah verilator="$1 -DVERILATOR=1 -sverilog -verbose -timescale=1ps/1ps" verilate CFLAGS="" LDFLAGS=""
