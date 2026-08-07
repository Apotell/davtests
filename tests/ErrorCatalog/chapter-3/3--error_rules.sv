/*
:name: chapter3_error_rules
:description: IEEE 1800-2023 Clause 3 error scenarios that parse cleanly
:tags: 3.12.1 3.13 3.14 3.14.2.2 3.14.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (sv_error_catalog_Latest.xlsx, sheet "Error Catalog"). The "catalog row N"
// comment is the link back to that row; the matching gtest is named RowN_...
//
// This file holds only the scenarios that parse without a syntax error, so a
// single compilation observes all of them at once. The scenarios that do raise
// syntax errors live in the sibling _inv*.sv fixtures, which are compiled in
// the same run but parsed independently, so their errors cannot damage this
// file.

// catalog row 4 | 3.12.1 | COMP
// Other than for task/function names, a reference shall only be made to a name
// already defined earlier in the compilation unit; "$unit::" only disambiguates
// and does not permit a forward reference.
task r4_t;
  int x;
  x = 5 + r4_b;
  x = 5 + $unit::r4_b;
endtask
bit r4_b;

// catalog row 5 | 3.13 | LINT
// Definitions name space: once a name defines a module, primitive, program or
// interface, that name shall not be used again for another one of those.
module r5_dup;
endmodule
interface r5_dup;
endinterface

// catalog row 6 | 3.13 | LINT
// Package name space: a package name shall not be used again to declare
// another package in any compilation unit.
package r6_p;
endpackage
package r6_p;
endpackage

// catalog row 7 | 3.13 | COMP
// Attribute name space: an attribute name is defined and usable only in the
// attribute name space; it is not visible as an ordinary identifier.
(* r7_fsm_state *) logic [3:0] r7_st;
module r7_m;
  initial $display(r7_fsm_state);
endmodule

// catalog row 8 | 3.13 | COMP
// Within a name space it shall be illegal to redeclare a name already
// declared by a prior declaration.
module r8_m;
  logic a;
  int a;
endmodule

// catalog row 9 | 3.14 | COMP
// The time precision shall be at least as precise as the time unit; it cannot
// be a coarser unit of time than the time unit.
module r9_m;
  timeunit 1ns / 1us;
endmodule

// catalog row 10 | 3.14.2.2 | COMP
// At most one time unit and one time precision per design element; a repeated
// declaration in the same time scope is legal only if it matches the first.
module r10_m;
  timeunit 1ns;
  logic a;
  timeunit 1ps;
endmodule

// catalog row 11 | 3.14.2.2 | COMP
// timeunit / timeprecision shall precede any other item in the time scope.
module r11_m;
  logic a;
  timeunit 1ns;
endmodule

// catalog row 13 | 3.14.3 | PARSE
// Unlike other time units, "step" cannot be used to set or modify the time
// unit or the time precision.
module r13_m;
  timeunit 1step;
endmodule

// catalog row 2 | 3.4 | PARSE
// A program block may contain only data declarations, class definitions,
// subroutine definitions, object instances and initial/final procedures. It
// cannot contain always procedures, primitive instances, module instances,
// interface instances or other program instances.
//
// Both offending items raise syntax errors, but the parser recovers locally --
// verified with a marker design element after this scenario, which still
// compiles -- so this belongs in the shared fixture, not a separate one.
module r2_m;
endmodule

program r2_prog;
  r2_m u1();
  always #5 ;
endprogram
