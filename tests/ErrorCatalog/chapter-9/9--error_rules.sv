/*
:name: chapter9_error_rules
:description: IEEE 1800-2023 Clause 9 error scenarios
:tags: 9.3.2
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml, path c:\MyWork\Projects\hlc\docs\error_catalog.xml).
// The "catalog row N" comment is the link back to that row; the matching
// gtest is named RowN_...
//
// This chapter currently has a single dut source file (dut_ch_9_3_2.sv) with
// one test case, so no sibling _inv*.sv fixtures are needed.

// catalog row 275 | 9.3.2 | COMP
// Within a fork-join_any or fork-join_none block it shall be illegal to
// refer to formal arguments passed by reference, other than in the
// initialization value expressions of variables declared in a
// block_item_declaration of the fork, unless the argument is declared ref
// static.
task r275_t(ref int r);
  fork
    r = 1;   // ILLEGAL: reference to a by-reference formal inside fork-join_none
  join_none
endtask
