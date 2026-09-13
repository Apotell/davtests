/*
:name: chapter14_error_rules
:description: IEEE 1800-2023 Clause 14 (Clocking blocks) error scenarios
:tags: 14.3 14.5 14.11 14.14 14.16
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

// catalog row 441 | 14.5 | COMP
// An expression assigned to a clocking signal in its declaration shall be
// legal as a port connection of the corresponding direction: input/inout
// expressions must be legal for a module input port, output/inout
// expressions must be legal for a module output port.
module r441_m;
  logic clk;
  logic [7:0] q;
  clocking cb @(posedge clk);
    output w = 8'hFF;
    input  r = q + 1;
  endclocking
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

// catalog row 450 | 14.14 | ELAB
// $global_clock resolution shall result in an error if no effective global
// clocking declaration is found in the enclosing instance scope or any
// ancestor up to and including a top-level hierarchy block.
module r450_top;
  r450_sub s();
endmodule
module r450_sub;
  always @($global_clock) begin
  end
endmodule

// catalog row 453 | 14.16 | PARSE
// The clockvar_expression on the left-hand side of a synchronous drive shall
// be a whole clockvar, a bit-select, or a slice; a concatenation is not
// allowed.
module r453_m;
  logic clk, a, b;
  clocking cb @(posedge clk);
    output a, b;
  endclocking
  initial {cb.a, cb.b} <= 2'b10;
endmodule
