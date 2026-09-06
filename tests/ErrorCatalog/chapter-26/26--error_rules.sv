/*
:name: chapter26_error_rules
:description: IEEE 1800-2023 Clause 26 (Packages) error scenarios
:tags: 26.3
*/

// catalog row 942 | 26.3 | COMP
// It shall be illegal if wildcard imports of more than one package within the
// same scope define the same potentially locally visible identifier and a
// reference resolves to that identifier.
package r942_p1;
  int c;
endpackage

package r942_p2;
  int c;
endpackage

module r942_m;
  import r942_p1::*;
  import r942_p2::*;
  initial c = 1;
endmodule
