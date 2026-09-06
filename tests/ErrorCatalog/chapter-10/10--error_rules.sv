/*
:name: chapter10_error_rules
:description: IEEE 1800-2023 Clause 10 (Assignment statements) error scenarios
:tags: 10.2 10.9
*/

// catalog row 290 | 10.2 | COMP
// The left-hand side of a continuous assignment shall be a net or variable, a
// CONSTANT bit-select or CONSTANT part-select of a vector net or packed
// variable, or a concatenation of those forms (Table 10-1); a non-constant
// select is illegal on the left-hand side of a continuous assignment.
module r290_m;
  wire [7:0] w;
  logic [2:0] idx;
  assign w[idx] = 1'b1;
endmodule

// catalog row 309 | 10.9 | COMP
// An assignment pattern expression shall not be used in a port expression in a
// module, interface, or program declaration.
//
// Kept last in this file: the offending expression sits in a module header, so
// if the parser cannot recover from it the loss is confined to the end of the
// file rather than taking row 290 with it.
module r309_m ( .p(int'{1}) );
endmodule
