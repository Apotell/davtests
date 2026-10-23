// Defaults that IEEE 1800-2023 Clause 37 records on an interface or a program.
//   Sec 16.15 / 37.6  default disable iff -> interface vpiDefaultDisableIff
//   Sec 14.12 / 37.6  default clocking    -> interface vpiDefaultClocking
//   Sec 14.14 / 37.6  global clocking     -> interface vpiGlobalClocking
//   Sec 14.12 / 37.9  default clocking    -> program vpiDefaultClocking

interface ifc(input logic clk, input logic rst);
  default disable iff (rst);
  default clocking cb @(posedge clk); endclocking
  global clocking gcb @(posedge clk); endclocking
endinterface

program prg(input logic clk);
  default clocking pcb @(posedge clk); endclocking
endprogram
