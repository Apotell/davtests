/*
:name: chapter30_error_rules
:description: IEEE 1800-2023 Clause 30 (Specify blocks) error scenarios
:tags: 30.4.1
*/

// catalog row 1010 | 30.4.1 | COMP
// The module path source shall be a net that is connected to a module input
// port or inout port. Using an internal net/variable that is not an input or
// inout port as the path source is illegal.
module r1010_m (input a, output q);
  wire internal;
  assign internal = a;
  specify
    (internal => q) = 10;
  endspecify
endmodule
