/*
:name: chapter26_error_rules
:description: IEEE 1800-2023 Clause 26 error scenarios
:tags: 26.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 942 | 26.3 | COMP
// It shall be illegal if wildcard imports of more than one package within
// the same scope define the same potentially locally visible identifier and
// a reference resolves to that identifier.
package r942_p1; int c; endpackage
package r942_p2; int c; endpackage
module r942_m;
  import r942_p1::*;
  import r942_p2::*;
  initial c = 1;   // illegal: c is potentially locally visible from two wildcard imports
endmodule
