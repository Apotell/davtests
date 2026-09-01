/*
:name: chapter6_error_rules_inv2
:description: IEEE 1800-2023 6.9.2 -- bit-select of a vectored net
:tags: 6.9.2
*/

// catalog row 13 | 6.9.2 | COMP
// If a net is declared with the vectored keyword, an implementation may
// (optionally) disallow bit-selects, part-selects, and strength
// specifications on that net.
//
// COMP_ILLEGAL_VECTORED_SELECT (WARNING, not ERROR): 6.9.2 says an
// implementation MAY disallow the select, not that it shall -- this is an
// optional restriction, not a violation, so a tool that permits it (as HLC
// currently does) is equally standard-conformant. The code exists only so
// the option has somewhere to report through if HLC ever chooses to enforce
// it; its %s names the select expression plus its enclosing design element.
//
// Kept in its own fixture: the catalog's own source used "logic vectored
// [15:0] a" (a variable, not a net), which HLC's grammar rejects outright --
// 'vectored' is a net_declaration-only keyword (IEEE 1800-2023 Annex A.2.1.3,
// net_declaration ::= ... [ vectored | scalared ] ...; it does not appear in
// the data_declaration production a 'logic' variable uses), so that spelling
// never reaches this row's rule at all. Rewritten below as a proper net
// declaration ("wire vectored") so the bit-select is the only thing under
// test.
module r13_m;
  wire vectored [15:0] r13_a;
  assign r13_a[1] = 1; // optional - an implementation MAY disallow this bit-select of a vectored net (6.9.2)
endmodule
