/*
:name: chapter14_error_rules
:description: IEEE 1800-2023 Clause 14 (Clocking blocks) error scenarios
:tags: 14.3 14.11
*/

// catalog row 439 | 14.3 | COMP
// It shall be illegal to read the value of any clockvar whose
// clocking_direction is output.
module r439_m;
  logic clk, b, r;
  clocking cb @(posedge clk);
    output b;
  endclocking
  initial r = cb.b;
endmodule

// catalog row 444 | 14.11 | COMP
// If no default clocking has been specified for the current module, interface,
// checker, or program, use of a ## cycle delay shall cause the compiler to
// issue an error. cb below is declared but never made the default.
module r444_m;
  logic clk;
  clocking cb @(posedge clk);
  endclocking
  initial begin
    ##5;
  end
endmodule
