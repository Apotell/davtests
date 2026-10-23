// IEEE 1800-2023 Sec 37.48 detail 1: a clocking block's vpiInputEdge/vpiOutputEdge
// come from its default skew, for every clocking block (default or not, in a
// module, interface or program).

module top(input logic clk);
  logic a, b;
  clocking cb_m @(posedge clk);
    default input #1step output negedge #2;
    input a;
    output b;
  endclocking
  default clocking cb_d @(posedge clk);
    default input posedge #1 output edge #1;
  endclocking
  clocking cb_none @(posedge clk);
    input a;
  endclocking
endmodule

interface ifc(input logic clk);
  logic c;
  clocking cb_i @(negedge clk);
    default input negedge output posedge;
    input c;
  endclocking
endinterface

program prg(input logic clk);
  clocking cb_p @(posedge clk);
    default output edge #3;
  endclocking
endprogram
