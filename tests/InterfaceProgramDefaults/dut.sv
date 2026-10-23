// Defaults that IEEE 1800-2023 Clause 37 records on an interface or a program.
//   Sec 16.15 / 37.6  default disable iff -> interface vpiDefaultDisableIff

interface ifc(input logic clk, input logic rst);
  default disable iff (rst);
endinterface
