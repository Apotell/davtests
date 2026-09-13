/*
:name: chapter25_error_rules
:description: IEEE 1800-2023 Clause 25 error scenarios
:tags: 25.3.3 25.6
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// Neither scenario raises a syntax error, so both are observed by a single
// compilation and no sibling fixture is needed.

// catalog row 906 | 25.3.3 | LINT
// An implicit port connection (.name or .*) cannot be used to reference a
// generic interface; a named port connection shall be used.
interface r906_simple_bus; logic req; endinterface
module r906_memMod (interface a, input logic clk);
endmodule
module r906_top;
  logic clk = 0;
  r906_simple_bus a();
  r906_memMod mem (.*); // illegal: generic interface port referenced by implicit port connection
endmodule

// catalog row 912 | 25.6 | LINT
// A ref port cannot be used as a terminal in a specify block; when an
// interface signal is accessed through a modport that gives it ref
// direction it is not a legal specify terminal.
interface r912_itf;
  logic c, q, d;
  modport flop (input c, ref d, output q);
endinterface
module r912_dtype (r912_itf.flop ch);
  specify
    (posedge ch.c => (ch.q +: ch.d)) = (5,6);
    $setup(ch.d, posedge ch.c, 1); // illegal: ch.d is a ref port, not usable as a specify terminal
  endspecify
endmodule
