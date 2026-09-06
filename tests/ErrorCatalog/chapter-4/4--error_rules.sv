/*
:name: chapter4_error_rules
:description: IEEE 1800-2023 Clause 4 error scenarios that parse cleanly
:tags: 4.9.6
*/

// Every scenario below is derived from one row of the SV error catalog
// (filtered set). The "catalog row N" comment is the link back to that row;
// the matching gtest is named RowN_...

// catalog row 4 | 4.9.6 | COMP
// Primitive (including UDP) output and inout terminals shall be connected
// directly to 1-bit nets or 1-bit structural net expressions, with no
// intervening process that could alter the strength. A multibit net or a
// non-structural expression on a primitive output terminal is illegal.
module r4_m;
  wire [1:0] r4_y;
  wire r4_a;
  not g1 (r4_y, r4_a);   // ILLEGAL: primitive output terminal must be a 1-bit net (4.9.6)
endmodule
