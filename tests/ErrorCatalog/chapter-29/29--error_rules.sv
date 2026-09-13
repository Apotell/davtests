/*
:name: chapter29_error_rules
:description: IEEE 1800-2023 Clause 29 error scenarios
:tags: 29.7
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 1005 | 29.7 | COMP
// The procedural assignment in a UDP initial statement shall assign to a
// reg whose identifier matches the identifier of the output port.
primitive r1005_srff (q, s, r);
  output q; reg q;
  input s, r;
  reg tmp;
  initial tmp = 1'b1;   // illegal: must assign to the output port reg q
  table
    1 0 : ? : 1 ;
  endtable
endprimitive
