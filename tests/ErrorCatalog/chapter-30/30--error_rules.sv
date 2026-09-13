/*
:name: chapter30_error_rules
:description: IEEE 1800-2023 Clause 30 error scenarios
:tags: 30.4.4.1
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 1013 | 30.4.4.1 | COMP
// The operands of a state-dependent path conditional expression shall be
// only: module input/inout ports (or their bit-/part-selects), locally
// defined variables or nets (or their selects), and compile-time constants
// (constant numbers and specparams). An output port or a hierarchical/
// external reference is not a legal operand.
module r1013_m (input a, input b, output q, output ctrl);
  specify
    if (ctrl) (a => q) = (2, 3);  // ILLEGAL: output port is not a legal state-dependent condition operand
  endspecify
endmodule
